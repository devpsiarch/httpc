#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define panic() fprintf(stderr, "%s\n", strerror(errno));exit(1);

void notfound(int *bytessent,int fd){
	char* res = "HTTP/1.1 404 Not Found\r\n\r\n";
	*bytessent = send(fd, res, strlen(res), 0);
}


void handel_getreq(char *reqpath,int fd,char **argv){
	
	int bytessent = 0;

	if(strcmp(reqpath,"/") == 0){
		char* res = "HTTP/1.1 200 OK\r\n\r\n";
		bytessent = send(fd, res, strlen(res), 0);
	}
	if(strncmp(reqpath,"/echo/",6) == 0){
		char *str = reqpath + 6;
		char res[1024];
		sprintf(res,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",strlen(str),str);
		bytessent = send(fd,res,strlen(res),0);
	}
	if(strncmp(reqpath,"/user-agent",11) == 0){
		strtok(NULL, "\r\n");
		strtok(NULL, "\r\n");
		char *user_agent = strtok(NULL,"\r\n") + 12;
		char res[1024];
		sprintf(res,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s",strlen(user_agent),user_agent);
		bytessent = send(fd,res,sizeof(res),0);
	}
	if(strncmp(reqpath,"/files/",7) == 0){
		if(strcmp(argv[1],"--directory") != 0){
			notfound(&bytessent,fd);
		}
		char *filename = reqpath + 7;
		char filepath[64];
		sprintf(filepath,"%s%s",argv[2],filename);
		FILE* file = fopen(filepath,"rb");
		if(file == NULL){
			fclose(file);
			notfound(&bytessent,fd);
		}	
		printf("opening file ...\n");

		if(fseek(file,0,SEEK_END) < 0){
			printf("error reading file!\n");
		}

		size_t size = ftell(file);

		//restor og pointer
		rewind(file);

		void* data = malloc(size);

		if(fread(data,1,size,file) != size){
			printf("error reading file\n");
		}

		char res[1024];
		sprintf(res,"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n%s",size,(char*)data);
		bytessent = send(fd,res,sizeof(res),0);
	}
	else{
		notfound(&bytessent,fd);
	}

	if(bytessent < 0){
		printf("sending failed !!!\n");
		panic();
	}
}

void handle_portreq(char *reqpath,char **argv,int fd,char readbuffer[1024]){
		reqpath = strtok(NULL," ");
		int bytessent;
		if(strncmp(reqpath,"/files/",7) == 0){
			if(strcmp(argv[1],"--directory") != 0){
				notfound(&bytessent,fd);
			}

			//parse filename
			char *filename = reqpath + 7;
			char *content = strdup(readbuffer); 

			//gets the size of the contents
			reqpath = strtok(NULL,"\r\n");	
			reqpath = strtok(NULL,"\r\n");
			reqpath = strtok(NULL,"\r\n");
			reqpath = strtok(NULL,"\r\n");
			reqpath = strtok(NULL,"\r\n"); //now reqpath holds content length : X
							
			char *strsize =	strtok(reqpath," ");
			strsize = strtok(NULL," ");

			int size = atoi(strsize);

			//getting the content
			content = strtok(content,"\r\n");
			content = strtok(NULL,"\r\n");
			content = strtok(NULL,"\r\n");
			content = strtok(NULL,"\r\n");
			content = strtok(NULL,"\r\n");//this holds the content ... i hope
			
			char filepath[64];
			sprintf(filepath,"%s%s",argv[2],filename);
			FILE* file = fopen(filepath,"wb");
			if(file == NULL){
				printf("error opening file");
				notfound(&bytessent,fd);
			}

			if(fwrite(content,1,size,file) != size){
				printf("error reading file");
			}	
			fclose(file);

			//response time			

			char *res = "HTTP/1.1 201 Created\r\n\r\n";
			bytessent = send(fd, res, strlen(res), 0);

		}		
}


int main(int argc,char **argv) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
	 	return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
	 	printf("SO_REUSEADDR failed: %s \n", strerror(errno));
	 	return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
	 								 .sin_port = htons(4221),
	 								 .sin_addr = { htonl(INADDR_ANY) },
	 								};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	 	printf("Bind failed: %s \n", strerror(errno));
	 	return 1;
	}
	
	int connection_backlog = 30;
	if (listen(server_fd, connection_backlog) != 0) {
	 	printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int fd = 0;	

	//threading using fork syscall	
	while(1){
		fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if(fd == -1){
			continue;
		}
		printf("connected ...");	
		int pid = fork();
		if(pid == 0){
			break;
		}
	}
	char readbuffer[1024];

	int bytes_rec = recv(fd,readbuffer,sizeof(readbuffer),0); 
	//this will more or less get the methode name
	char* reqpath = strtok(readbuffer," ");
	if(strcmp(reqpath,"GET") == 0){
		reqpath = strtok(NULL," ");
		handel_getreq(reqpath,fd,argv);	
	}
	else if(strcmp(reqpath,"POST") == 0){	
		handle_portreq(reqpath,argv,fd,readbuffer[1024])
	}

	close(server_fd);

	return 0;
}
