
//this code is an implementation of the DV algorithm.
//see README
// remove // from prints to show all the work the algorithm does,
//-------------header files-------------------------------------------------------------------------//
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>//Internet address family
#include <netdb.h>//definitions for network database operations
#include <arpa/inet.h>//definitions for internet operations
#include <sys/types.h>
#include <sys/socket.h>
#include  <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
//--------------------------------------------------------------------------------------------//




//--------------------------------------------------------------------------------------------//
#define INITIAL_SIZE 10//initial size of 'buffer' to read dynamically from file.
#define INFINITY 99999//represents infinity
//--------------------------------------------------------------------------------------------//



//structs for the algorithm
//--------------------------------------------------------------------------------------------//
//this struct represents a single router
typedef struct Node
{
  char* name;
  char* ip;
  int port;
}Node;
//this struct represents all the data needed for a router's routing table.
typedef struct RoutingTable
{
  int numOfNodes;//number of routers in the system
  Node* nodes;//list of all the routers in the system
  int** r_table;//the routing table values
  Node* table_node;//the source router
  int* via;//list of all the the first nodes in the routes between the source router to every other router in the system
  int* directNeig;//list of all direct neighbors of source router
  int directNeighbors;//number of all direct neighbors of source router
}RT;
//--------------------------------------------------------------------------------------------//



//global variables
//--------------------------------------------------------------------------------------------//
pthread_t* threads;//array of threads
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t convar1=PTHREAD_COND_INITIALIZER;
pthread_cond_t convar2=PTHREAD_COND_INITIALIZER;
pthread_cond_t convar3=PTHREAD_COND_INITIALIZER;
RT* routing_table;
char* file_char;//a char array which contains all the file data .in this way we only once make an i/o operation
//to the disk.
int numOfAttempts=0;//number of attempts to connect to other router
int sent=0;//number of sent DVs
int received=0;//number of received DVs
int zeroReceived=0;//number of 0 received
int changed=1;//boolean variable which indicates if source vector's DV changed.
int doneComputing=0;//boolean variable which indicates if the recalculation is done
int numOfSentThreadExists=0;//number of send threads that are exist.
int numOfReceivedThreadExists=0;//number of receive threads that are exist.
int isRecomputeExists=0;//boolean variable which indicates if the calculation thread is exists.
int done=0;
//--------------------------------------------------------------------------------------------//



//methods
//--------------------------------------------------------------------------------------------//
void clean();
void copyFileToString(char* file);
void setInitData(char* router_name);
char* getCharToSpace(char* src);
int getNodeIndex(char* node);
void initialize();
void printTable();
int sendDV(Node* node);
int asciiSum(char* name);
int recomputeDV();
void printFinalResualt();
int* getAllDirectNeighbors(int x);
void* rcvDV(char* name);
void Bellman_Ford();
int min(int x,int y);
int D(int x,int y);
int c(int x,int y);
void freeNode(Node node);
void freeRT(RT* rt);
int getNumOfDirectNeighbors(int x);
int checkIfChanged(int* last_dv,int* new_dv);
int getTid(pthread_t pt);
//--------------------------------------------------------------------------------------------//



//main
//--------------------------------------------------------------------------------------------//
int main(int argc, char **argv)
{
  copyFileToString(argv[1]);
  setInitData(argv[2]);
  numOfAttempts=atoi(argv[3]);

  Bellman_Ford();
  return 0;
}
//--------------------------------------------------------------------------------------------//



//this method prints the "via des cost" to all of the other routers
//--------------------------------------------------------------------------------------------//
void printFinalResualt(){
  int i=0;
  int myIndex=getNodeIndex(routing_table->table_node->name);
  for(i=0;i<routing_table->numOfNodes;i++){
      int last=routing_table->via[i];
      while(routing_table->via[last]!=last)
        last=routing_table->via[last];
      printf("%s\t%s\t%d\n",routing_table->nodes[i].name,routing_table->nodes[last].name,routing_table->r_table[myIndex][i]);
  }
}
//--------------------------------------------------------------------------------------------//



//this method frees all the dynamically allocated memory
//--------------------------------------------------------------------------------------------//
void clean(){
 // printf("done\n");
  freeRT(routing_table);
  free(file_char);
  free(threads);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&convar1);
  pthread_cond_destroy(&convar2);
  pthread_cond_destroy(&convar3);
}
//--------------------------------------------------------------------------------------------//



//this method gets the tid of a thread
//--------------------------------------------------------------------------------------------//
int getTid(pthread_t pt){
  int i=0;
  int n=2*routing_table->directNeighbors;
  for (i=0;i<n;i++){
      if(pthread_equal(threads[i], pt)!=0){
          return i;
      }
  }
  return -1;
}
//--------------------------------------------------------------------------------------------//



//this method frees a node
//--------------------------------------------------------------------------------------------//
void freeNode(Node node){
  free(node.name);
  free(node.ip);
}
//--------------------------------------------------------------------------------------------//



//this method frees a routing table struct
//--------------------------------------------------------------------------------------------//
void freeRT(RT* rt){
  int i=0;
  while(i<routing_table->numOfNodes){
      free(rt->r_table[i]);
      freeNode(rt->nodes[i]);
      i++;
  }
  free(rt->r_table);
  free(rt->nodes);
  free(rt->via);
  free(rt->directNeig);
  free(rt);
}
//--------------------------------------------------------------------------------------------//



//this method sets the initiate data, like the number of routers ,allocates needed memory..
//--------------------------------------------------------------------------------------------//
void setInitData(char* node_name){

  char *token;
  const char* line="\n";
  const char space[3]=" ";
  routing_table=(RT*)malloc(sizeof(RT));

  /* get the first token */
  char* tmp1=getCharToSpace(file_char);
  routing_table->numOfNodes=atoi(tmp1);
  free(tmp1);
  routing_table->r_table=(int**)malloc(sizeof(int*)*routing_table->numOfNodes);
  int k=0;
  int j=0;
  for(k=0;k<routing_table->numOfNodes;k++){
      routing_table->r_table[k]=(int*)malloc(sizeof(int)*routing_table->numOfNodes);
      for(j=0;j<routing_table->numOfNodes;j++)
        routing_table->r_table[k][j]=INFINITY;
  }

  //-------------------------------------------------------------//
  //

  routing_table->nodes=(Node*)malloc(sizeof(Node)*routing_table->numOfNodes);
  routing_table->via=(int*)malloc(sizeof(int)*routing_table->numOfNodes);
  //


  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  //getting all the information about the routers in the system
  j=0;
  char* tmpToken;
  token = strstr(file_char,line);
  while(j<routing_table->numOfNodes)
    {

      routing_table->nodes[j].name=(getCharToSpace((token+1)));
      tmpToken=strstr(token,space);
      routing_table->nodes[j].ip=(getCharToSpace((tmpToken+1)));
      tmpToken=strstr((tmpToken+1),space);
      char* tmp;
      tmp=(getCharToSpace((tmpToken+1)));
      routing_table->nodes[j].port=atoi(tmp);
      free(tmp);
      if(strstr((token+1),line)!=NULL)
        token = strstr((token+1),line);

      if(strcmp(routing_table->nodes[j].name,node_name)==0){
          routing_table->table_node=&routing_table->nodes[j];
      }

      j++;

    }
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //

}//
//--------------------------------------------------------------------------------------------//



//this method computes the ASCII sum a char array
//--------------------------------------------------------------------------------------------//
int asciiSum(char* name){
  int sum=0;
  int i=0;
  while(i<strlen(name)){
      sum+=name[i];
      i++;
  }
  return sum;
}
//--------------------------------------------------------------------------------------------//



//this method gets the index of a Node by it's name.
//--------------------------------------------------------------------------------------------//
int getNodeIndex(char* node){
  int i=0;
  while(i<routing_table->numOfNodes){
      if(strcmp(routing_table->nodes[i].name,node)==0)
        return i;
      i++;
  }
  return -1;
}
//--------------------------------------------------------------------------------------------//



//this method initialize the data from the file into the RT strcut
//--------------------------------------------------------------------------------------------//
void initialize(){
  char *token;
  const char* line="\n";
  const char space[3]=" ";
  int i=0;
  token = strstr(file_char,line);
  while(i<routing_table->numOfNodes){
      token = strstr(token+1,line);
      i++;
  }
  char* tmpToken;
  char* src;
  char* des;
  char* cost;


  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  //getting all the information about the edges between the routers in the system
  while(1){
      if(*(token+1)<65)
        break;

      src=(getCharToSpace((token+1)));
      tmpToken=strstr(token,space);
      des=(getCharToSpace((tmpToken+1)));
      tmpToken=strstr((tmpToken+1),space);
      cost=(getCharToSpace((tmpToken+1)));
      if(strcmp(src,routing_table->table_node->name)==0)
        routing_table->r_table[getNodeIndex(src)][getNodeIndex(des)]=atoi(cost);
      else if(strcmp(des,routing_table->table_node->name)==0)
        routing_table->r_table[getNodeIndex(des)][getNodeIndex(src)]=atoi(cost);
      free(src);
      free(des);
      free(cost);
      if(strstr(token+1,line)==NULL ||strlen(strstr(token+1,line))<=1)
        break;

      token = strstr(token+1,line);
  }
  routing_table->r_table[getNodeIndex(routing_table->table_node->name)][getNodeIndex(routing_table->table_node->name)]=0;
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  //setting via array
  int j=0;
  for(j=0;j<routing_table->numOfNodes;j++){
      if(routing_table->r_table[getNodeIndex(routing_table->table_node->name)][j]==INFINITY)
        routing_table->via[j]=INFINITY;
      else
        routing_table->via[j] =j;
  }
  //setting info about neighbors
  routing_table->directNeighbors=getNumOfDirectNeighbors(getNodeIndex(routing_table->table_node->name));
  routing_table->directNeig=getAllDirectNeighbors(getNodeIndex(routing_table->table_node->name));
}
//--------------------------------------------------------------------------------------------//



//this method returns the number of direct neighbors of a router
//--------------------------------------------------------------------------------------------//
int getNumOfDirectNeighbors(int x){
  int i=0;
  int counter=0;
  while(i<routing_table->numOfNodes){
      if(routing_table->r_table[x][i]!=INFINITY && i!=x)
        counter++;
      i++;
  }
  return counter;
}
//--------------------------------------------------------------------------------------------//



//this method return substring from a source string ,until a space char funded in the source string
//--------------------------------------------------------------------------------------------//
char* getCharToSpace(char* src){
  int n=0;
  while(src[n]!=' ' && src[n]!='\0'&& src[n]!='\n'){
      n++;
  }
  char* res=(char*)malloc(n+1);
  int i=0;
  while(i<n){
      res[i]=src[i];
      i++;
  }

  res[i]='\0';
  return res;
}
//--------------------------------------------------------------------------------------------//



//this method prints the routing table
//--------------------------------------------------------------------------------------------//
void printTable(){
  int i=0;
  int j=0;
  printf("\t");
  for(i=0;i<routing_table->numOfNodes;i++)
    printf("%s\t",routing_table->nodes[i].name);
  i=0;
  printf("\n");
  for(i=0;i<routing_table->numOfNodes;i++){
      printf("%s\t",routing_table->nodes[i].name);
      for(j=0;j<routing_table->numOfNodes;j++){
          printf("%d\t",routing_table->r_table[i][j]);
      }
      printf("\n");
  }

}
//--------------------------------------------------------------------------------------------//



//this method calculates does 'relax'.
//--------------------------------------------------------------------------------------------//
int D(int x,int y){
  int i=0;
  int myIndex=getNodeIndex(routing_table->table_node->name);

  int min_value=INFINITY;
  for(i=0;i<routing_table->directNeighbors;i++){
      min_value=min(min_value, c(myIndex,routing_table->directNeig[i])+c(routing_table->directNeig[i],y) );
      if(c(myIndex,y)>c(myIndex,routing_table->directNeig[i])+c(routing_table->directNeig[i],y))//doing relax
        routing_table->via[y]=routing_table->directNeig[i];
  }
  return min_value;
}
//--------------------------------------------------------------------------------------------//



//this method returns the cost of a edge between x to y.
//--------------------------------------------------------------------------------------------//
int c(int x,int y){
  return routing_table->r_table[x][y];
}
//--------------------------------------------------------------------------------------------//



//this method returns minimum
//--------------------------------------------------------------------------------------------//
int min(int x,int y){
  if(x>y)
    return y;
  return x;
}
//--------------------------------------------------------------------------------------------//



//this method returns all a array of indexes of the direct neighbors of a router
//--------------------------------------------------------------------------------------------//
int* getAllDirectNeighbors(int x){
  int i=0;
  int j=0;
  int myIndex=getNodeIndex(routing_table->table_node->name);
  int* neighbors=(int*)malloc(sizeof(int)*routing_table->directNeighbors);
  for(i=0;i<routing_table->numOfNodes;i++){
      if(routing_table->r_table[myIndex][i]!=INFINITY && i!=myIndex){
          neighbors[j]=i;
          j++;
      }

  }
  return neighbors;
}
//--------------------------------------------------------------------------------------------//



//this method return a char array which has a copy of all the input file data ; it does it dynamically.
//--------------------------------------------------------------------------------------//
void copyFileToString(char* file){
  //------opening file--------------------------------------------------------------------------------//
  FILE* fp;
  fp = fopen(file,"r");
  if( fp == NULL )
    {
      printf("%s\n", strerror(errno));
      exit(0);
    }
  //--------------------------------------------------------------------------------------//

  //-----initializing variables---------------------------------------------------------------------------------//
  file_char=(char*)malloc(INITIAL_SIZE);
  char tmp[INITIAL_SIZE];
  int file_char_length=INITIAL_SIZE;
  int tmp_length=INITIAL_SIZE;
  bzero(file_char,file_char_length);
  bzero(tmp,tmp_length);
  int bytes=0;
  int n=0;
  file_char[0]='\0';
  while(1){
      //--------reading data dynamically------------------------------------------------------------------------------//
      bzero(tmp,tmp_length);
      n=fread(tmp, 1, tmp_length-1, fp);
      if(n<0){
          printf("%s\n", strerror(errno));
          exit(0);
      }
      if(n==0)
        break;

      bytes+=n;
      //---checking if the buffer has enough length to contain the part of the message---------------------------------------------------------------------------------//
      if(bytes<file_char_length){
          strcat(file_char,tmp);
      }
      //----checking if the buffer does not has enough length to contain the part of the message
      //therefore it need to be realloced----------------------------------------------------------------------------------//
      if(bytes>=file_char_length){
          file_char_length=bytes;
          file_char=(char*)realloc(file_char,file_char_length+1);
          strcat(file_char,tmp);
      }
  }

  //--------------------------------------------------------------------------------------//
  fclose(fp);

}
//--------------------------------------------------------------------------------------------//



//this method runs the DV algorithm
//--------------------------------------------------------------------------------------------//
void Bellman_Ford(){
  initialize();


  int i,j,rc;
  int myIndex;
  i=0;
  j=0;
 // printTable();

 // printf("num of neig%d\n",routing_table->directNeighbors);
  threads=malloc(sizeof(pthread_t)*routing_table->directNeighbors*2);
  myIndex=getNodeIndex(routing_table->table_node->name);


  //creating thread for calculation
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  pthread_t recompute;
  rc=pthread_create(&recompute,NULL,(void*)recomputeDV,NULL);
  if(rc){
      printf("%s\n", strerror(errno));
      exit(0);
  }
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


  //creating threads for sending
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  while(i<routing_table->directNeighbors){
      rc=pthread_create(&threads[j],NULL,(void*)rcvDV,routing_table->nodes[routing_table->directNeig[i]].name);
      if(rc){
          printf("%s\n", strerror(errno));
          exit(0);
      }
      j++;
      i++;
  }
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


  //creating threads for receiving
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  i=0;
  while(i<routing_table->directNeighbors){
      //  if(routing_table->r_table[myIndex][i]!=999 &&i!=myIndex){

      //sending
      rc=pthread_create(&threads[j],NULL,(void*)sendDV,&routing_table->nodes[routing_table->directNeig[i]]);

      if(rc){
          printf("%s\n", strerror(errno));
          exit(0);
      }
      j++;
      i++;
  }
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


  //Synchronizing the sending's ,receving's and recomputing's mutex and conditional variables
  //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
  int sentOnce=0;
  //waiting for all the threads to be exist and go into wait mode
  while(numOfSentThreadExists<routing_table->directNeighbors||numOfReceivedThreadExists<routing_table->directNeighbors||isRecomputeExists==0);

  while(1){
      pthread_mutex_lock(&mutex);


      //waking all the sending threads.
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
      if(sent==0 &&sentOnce==0 ){
      //    printf("Bellman_Ford():signal sent to convar1\n");
          sentOnce=1;
          for(i=0;i<routing_table->directNeighbors;i++){

              pthread_cond_signal(&convar1);
              pthread_mutex_unlock(&mutex);

          }
          pthread_mutex_lock(&mutex);
      }
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


      //waking all the sending threads.
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
//      if(sent==routing_table->directNeighbors){
//
////          printf("Bellman_Ford():signal sent to convar2\n");
// //         sent=-1;
////          for(i=0;i<routing_table->directNeighbors;i++){
////              pthread_cond_signal(&convar2);
////          }
////          pthread_mutex_unlock(&mutex);
////          pthread_mutex_lock(&mutex);
//      }
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
      //in the case which all of the other nodes sent 0 then finishing the algorithm
      if(zeroReceived>=routing_table->directNeighbors){
          received=-1;
          sent=-1;
    //      printf("Bellman_Ford():signal sent to convar1+2+3\n");
          for(i=0;i<routing_table->directNeighbors;i++){
              pthread_cond_signal(&convar1);
          }
//          for(i=0;i<routing_table->directNeighbors;i++){
//              pthread_cond_signal(&convar2);
//          }

          pthread_cond_signal(&convar3);
          pthread_mutex_unlock(&mutex);

          pthread_mutex_lock(&mutex);
      //    printf("Bellman_Ford():done=1\n");

          break;
      }
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
      //in the case which all neighbors's DV received ,waking the recomputing thread.
      if(received>=routing_table->directNeighbors&& sent>=routing_table->directNeighbors){

         // printf("Bellman_Ford():signal sent to convar3\n");
          received=-1;
          sent=-1;
          pthread_cond_signal(&convar3);
          pthread_mutex_unlock(&mutex);
          pthread_mutex_lock(&mutex);
      }
      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //


      //^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ ^_^ //
      //reset all the global variables
      if(doneComputing==1){
          received=0;
          sent=0;
          sentOnce=0;
          doneComputing=0;
      }


      pthread_mutex_unlock(&mutex);
  }
 // printf("Bellman_Ford():outside main loop\n");

  pthread_mutex_unlock(&mutex);









  //joining all the threads-this means the main thread will wait until all of the threads will exist.
  pthread_join(recompute, NULL);
  for (i=0; i<routing_table->directNeighbors*2; i++) {
      pthread_join(threads[i], NULL);
  }



 // printTable();
 // printf("printing via\n\n\n");
//  for(i=0;i<routing_table->numOfNodes;i++)
//    printf("%d\t",routing_table->via[i]);
//  printf("\n\n\n");
  printFinalResualt();//printing resualt
//  printf("\n\n\n");
  clean();//cleaning all the dynamically allocated memory,

  exit(0);
}
//--------------------------------------------------------------------------------------------//



//this method checks if the source DV changed
//--------------------------------------------------------------------------------------------//
int checkIfChanged(int* last_dv,int* new_dv){
  int i=0;
  for(i=0;i<routing_table->numOfNodes;i++){
      if(new_dv[i]!=last_dv[i])
        return 0;
  }
  return -1;
}
//--------------------------------------------------------------------------------------------//



//this method sends DV to a router
//--------------------------------------------------------------------------------------------//
int sendDV(Node* node){

  //printf("sendDV():thread :%d created for %s\n",getTid(pthread_self()),node->name);
  //------opening TCP  socket--------------------------------------------------------------------------------//
  int socket_fd= socket(AF_INET,SOCK_STREAM,0);
  if(socket_fd<0){
      printf("%s\n", strerror(errno));
      return 0;
  }
  //--------------------------------------------------------------------------------------//

  //--------filling struct sockaddr_in gmail_server in the ip and the port of the server (from struct hostent* gmail_info)------------------------------------------------------------------------------//
  struct sockaddr_in neighbor;//this struct contains ip address and a port of the server.
  bzero(&neighbor,sizeof(neighbor));
  neighbor.sin_family=AF_INET;//AF_INIT means Internet doamin socket.
  neighbor.sin_port=htons(node->port+asciiSum(routing_table->table_node->name));//port 25=SMTP.
  neighbor.sin_addr.s_addr = inet_addr(node->ip);//converts char ip to s_addr.
  //--------------------------------------------------------------------------------------//
  int i=0;
  int con;
  while(i<numOfAttempts){
      //----connecting to to the server via above socket----------------------------------------------------------------------------------//
      con=connect(socket_fd,(struct sockaddr *)&neighbor,sizeof(neighbor));
      if(con<0)
        sleep(1);
      else
        break;
      i++;
  }
  if(con<0){
      printf("%s\n", strerror(errno));
      return 0;
  }

  //--------------------------------------------------------------------------------------//
  while(1)
    {
      pthread_mutex_lock(&mutex);
      numOfSentThreadExists++;
      if(zeroReceived>=routing_table->directNeighbors)
        break;
      pthread_cond_wait(&convar1,&mutex);
      if(zeroReceived>=routing_table->directNeighbors)
        break;
    //  printf("sendDV():  trying to send to:%s\n",node->name);

      int n=0;
      int j=0;
      if(changed==1)
        {
          //sending 1|dv.
          int* d_vector=(int*)malloc((1+routing_table->numOfNodes) *sizeof(int));
          d_vector[0]=1;
          for(j=1;j<routing_table->numOfNodes+1;j++)
            d_vector[j]=routing_table->r_table[getNodeIndex(routing_table->table_node->name)][j-1];
          n=write(socket_fd, d_vector, ((1+routing_table->numOfNodes) *sizeof(int)));
          free(d_vector);

          if(n<0){
           //   printf("%s\n", strerror(errno));
              return 0;
          }
      //    printf("sendDV():  DV sent to:%s\n",node->name);

        }


      else
        {
          //sending zero
          int zero=0;
          n=write(socket_fd, &zero, sizeof(int));

        //  printf("sendDV():  ZERO sent to:%s\n",node->name);

          if(n<0){
              printf("%s\n", strerror(errno));
              return 0;
          }
        }

      sent++;
      //--------------------------------------------------------------------------------------//

      //--------------------------------------------------------------------------------------//
      pthread_mutex_unlock(&mutex);

      //--------------------------------------------------------------------------------------//
      //sleep(1);
    }


  //closing socket
  int socket_close=close(socket_fd);
  if(socket_close==-1){
      printf("%s\n", strerror(errno));
      return 0;
  }
  //printf("sendDV():oudside main loop\n");
  pthread_mutex_unlock(&mutex);

  return 0;

  return 0;
}
//--------------------------------------------------------------------------------------------//




//--------------------------------------------------------------------------------------------//
void* rcvDV(char* name){

  //printf("rcvDV():thread number:%d created for:%s\n",getTid(pthread_self()),name);
  //------opening TCP  socket--------------------------------------------------------------------------------//
  int socket_fd= socket(AF_INET,SOCK_STREAM,0);
  if(socket_fd<0){
      printf("%s\n", strerror(errno));
      return 0;
  }
  //--------------------------------------------------------------------------------------//

  //--------filling struct sockaddr_in  in the ip and the port of the server (from struct hostent* gmail_info)------------------------------------------------------------------------------//
  struct sockaddr_in myAddress;
  bzero(&myAddress,sizeof(myAddress));
  myAddress.sin_family=AF_INET;//AF_INIT means Internet doamin socket.
  myAddress.sin_port=htons(routing_table->table_node->port+asciiSum(name));//port 25=SMTP.
  myAddress.sin_addr.s_addr=inet_addr(routing_table->table_node->ip);
  //--------------------------------------------------------------------------------------//

  //--------------------------------------------------------------------------------------//
  ///binding the socket to the TCP port which it's listen to
  //in other words defining the new socket as the information of neighbor
  int bind_socket=bind(socket_fd,(struct sockaddr*)&myAddress,sizeof(myAddress));
  if(bind_socket==-1){
      printf("%s\n", strerror(errno));
      return 0;
  }

  //initiating listening and defining the max length of connection queue to only 5
  //connection
  int listen_socket=listen(socket_fd,routing_table->numOfNodes);
  if(listen_socket==-1){
      printf("%s\n", strerror(errno));
      return 0;
  }

  //accepting connection requests and delete them from the request queue
  //the client information is returned and placed by the function 'accept'
  //to the struct 'client_addr'
  //also the function returns the socket fd of the client
  struct sockaddr_in client_addr;
  socklen_t client_length=sizeof(client_addr);
  int client_socket_fd=accept(socket_fd,(struct sockaddr*)&client_addr,&client_length);
  if(client_socket_fd==-1){
      printf("%s\n", strerror(errno));
     // printTable();
      return 0;
  }




  while(1){

      //--------------------------------------------------------------------------------------//
      pthread_mutex_lock(&mutex);
      numOfReceivedThreadExists++;
      if(zeroReceived>=routing_table->directNeighbors)
        break;
      pthread_mutex_unlock(&mutex);

      //--------------------------------------------------------------------------------------//

     // printf("rcvDV():trying to receive from:%s\n",name);



      //-----receiving---------------------------------------------------------------------------------//

      int k=0;
      //-------writing the data to the server and printing it to the screen-------------------------------------------------------------------------------//
      int* dv=calloc(sizeof(int),routing_table->numOfNodes+1);
      int n=read(client_socket_fd, dv, ((1+routing_table->numOfNodes) *sizeof(int)));
      if(n<0){
          printf("%s\n", strerror(errno));
          return 0;
      }
      //--------------------------------------------------------------------------------------//
      pthread_mutex_lock(&mutex);
      if(received<=routing_table->directNeighbors){
      k=0;
      if(dv[0]==0)
        {
          //did get zero

          zeroReceived++;
        //  printf("rcvDV():ZERO received from:%s\n",name);

        }
      else
        {
          //did'nt get zero
          k=0;
          for(k=1;k<routing_table->numOfNodes+1;k++)
            routing_table->r_table[getNodeIndex(name)][k-1]=dv[k];
       //   printf("rcvDV():dv received from:%s\n",name);

        }
      received++;
      }
      pthread_mutex_unlock(&mutex);

      free(dv);



      //--------------------------------------------------------------------------------------//
      //sleep(1);

  }
  //printf("rcvDV():oudside main loop\n");

  //closing socket
  int socket_close=close(socket_fd);
  if(socket_close==-1){
      printf("%s\n", strerror(errno));
      return 0;
  }
  pthread_mutex_unlock(&mutex);
  return 0;
  return NULL;
}
//--------------------------------------------------------------------------------------------//




//--------------------------------------------------------------------------------------------//
int recomputeDV(){
  isRecomputeExists=1;


  while(1)
    {

      pthread_mutex_lock(&mutex);
      if(zeroReceived>=routing_table->directNeighbors)
        break;

      pthread_cond_wait(&convar3,&mutex);
      if(zeroReceived>=routing_table->directNeighbors)
        break;

      //-------recomputing-------------------------------------------------------------------------------//
      //          receivedZeroCounter=0;
      //printf("recomputeDV():recomputing\n");
      int i=0;
      int myIndex;
      int* lastDV=(int*)malloc(sizeof(int)*routing_table->numOfNodes);
      myIndex=getNodeIndex(routing_table->table_node->name);
      for(i=0;i<routing_table->numOfNodes;i++){
          lastDV[i]=routing_table->r_table[myIndex][i];
          if(i!=myIndex)
            routing_table->r_table[myIndex][i]=D(myIndex,i);



      }
    //  printf("recomputeDV():recomputed\n");
      //checking if changed
      if(checkIfChanged(lastDV,routing_table->r_table[myIndex])==0)
        {
          changed=1;
        }
      else
        {
          //if did'nt changed
          changed=0;
        }
      doneComputing=1;
     // printTable();
      free(lastDV);
      pthread_mutex_unlock(&mutex);
      //sleep(1);
    }


  //exiting
  //printf("recomputeDV():oudside main loop\n");

  pthread_mutex_unlock(&mutex);
  return 0;


}
//--------------------------------------------------------------------------------------------//
