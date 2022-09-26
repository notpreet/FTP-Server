#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <ftw.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#define MAX 80
#define PORT 8080
#define SA struct sockaddr
int connfd, status = 0, port_no = -1, data_socket, data_fd;
int is_port = 0;
char buff[MAX];
char *rename_file_from;
struct sockaddr_in addr;

char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0)
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	// Write new null terminator character
	end[1] = '\0';

	return str;
}
int delete_files(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int removed_file = remove(fpath);

	if (removed_file)
		perror(fpath);

	return removed_file;
}

int rmrf(char *path)
{
	return nftw(path, delete_files, 64, FTW_DEPTH | FTW_PHYS);
}
void stor(int connfd)
{
	// checking if the port is opend before storing
	// 200 when successfully executed
	// 501 if port not open
	// 502 when file creation problem occurs
	if (is_port == 1)
	{
		char *cmd = strtok(buff, " ");
		char *fname = strtok(NULL, " ");
		// removing \n in fname
		fname[strlen(fname) - 1] = '\0';
		int addrlen = sizeof(addr);
		// acccepting data into new socket
		data_socket = accept(data_fd, (struct sockaddr *)&addr, (socklen_t *)&addr);
		char b[1024];
		// reading data
		int valread = read(data_socket, b, 1024);
		// writing into file
		int fd = open(fname, O_CREAT | O_WRONLY, 0777);
		if (fd == -1)
			write(connfd, "502", 4);
		else
		{
			// successfully written
			int x = write(fd, b, sizeof(b));
			write(connfd, "200\n", 4);
		}
	}
	else
	{
		// when port is not set
		write(connfd, "500\n", 4);
	}
}
void port(int connfd)
{
	// 200  when exected
	// 501 when socket creation failed
	// 502 when binding fails
	// 503 when listening failed
	char *cmd = strtok(buff, " ");
	char *prt = strtok(NULL, " ");
	// removing \n in path
	prt[strlen(prt) - 1] = '\0';
	port_no = atoi(prt);
	int addrlen = sizeof(addr);
	if ((data_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		write(connfd, "501", 6);
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port_no);
	if (bind(data_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		write(connfd, "502", 6);
	}
	if (listen(data_fd, 3) < 0)
	{
		write(connfd, "503", 6);
	}
	write(connfd, "200\n", 4);
}
void retr(int connfd)
{
	//200 when successful
	//500 when port not set for data
	//501 when file not found
	if (is_port == 1)
	{
		char *cmd = strtok(buff, " ");
		char *fname = strtok(NULL, " ");
		// removing \n in fname
		fname[strlen(fname) - 1] = '\0';
		// listening on the socket
		int addrlen = sizeof(addr);
		// accepting data on data socket
		data_socket = accept(data_fd, (struct sockaddr *)&addr, (socklen_t *)&addr);
		char b[1024];
		int valread = read(data_socket, b, 1024);
		int fd = open(fname, O_RDONLY);
		if (fd == -1)
			write(connfd, "501\n", 4);
		else
		{
			// creating buffer to read file
			char b[1024];
			// seeking to end to calculate  the size
			long fsize = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);
			// allocating the memory
			char *string = malloc(fsize + 1);
			// reading the data
			read(fd, string, fsize);
			// sending data on data socket
			send(data_fd, string, strlen(string), 0);
			// closing the file after use
			fclose(fd);
		}
		write(connfd, "200\n", 4);
	}
	else
	{
		write(connfd, "500\n", 4);
	}
}
void rnfr(int connfd)
{
	//200 when successful
	// retreiving the command from the buffer
	char *cmd = strtok(buff, " ");
	char *rnff = strtok(NULL, " ");
	rename_file_from = malloc(sizeof(rnff));
	strcpy(rename_file_from, rnff);
	write(connfd, "200\n", 4);
}
void rnto(int connfd)
{
	//200 when successful
	//500 when rename error occurs
	char *cmd = strtok(buff, " ");
	char *rename_to = strtok(NULL, " ");
	rename_file_from = trimwhitespace(rename_file_from);
	rename_to = trimwhitespace(rename_to);
	if (rename(rename_file_from, rename_to) != 0)
	{
		write(connfd, "200\n", 4);
	}
	else
		write(connfd, "500\n", 4);
}
void dele(int connfd)
{
	//200 when successful
	// 500 when error occurs
	char *cmd = strtok(buff, " ");
	char *filename = strtok(NULL, " ");
	filename[strlen(filename) - 1] = '\0';
	if (remove(filename) == 0)
		write(connfd, "200\n", 4);
	else
		write(connfd, "500\n", 4);
}
void rmd(int connfd)
{
	char *cmd = strtok(buff, " ");
	char *directory = strtok(NULL, " ");
	directory[strlen(directory) - 1] = '\0';
	// removing all files recursively and then removing the directory itself
	rmrf(directory);
	write(connfd, "200\n", 4);
}
void mkd(int connfd)
{
	//200 when successful
	//500 when failed
	char *cmd = strtok(buff, " ");
	char *directory = strtok(NULL, " ");
	directory[strlen(directory) - 1] = '\0';
	if (mkdir(directory, 0777) != -1)
	{
		write(connfd, "200\n", 4);
	}
	else
		write(connfd, "500\n", 4);
}
void pwd(int connfd)
{
	//send the cwd when successful
	//500 when error occurs
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != 0)
	{
		int len = strlen(cwd);
		write(connfd, cwd, len);
	}
	else
		write(connfd, "500\n", 4);
}
void list(int connfd)
{
	//returns files in current working directory 
	DIR *dp;
	struct dirent *dirp;
	dp = opendir("./");
	char *list = (char *)malloc(2048);
	int len = 0;
	while ((dirp = readdir(dp)) != NULL)
	{
		char *files = dirp->d_name;
		strcat(files, "\n");
		len += strlen(files);
		strcat(list, files);
	}
	write(connfd, list, len);
	close(dp);
	free(list);
}
// Function designed for communication between the server and the client
void func(int connfd)
{

	int n;

	// infinite loop for chat
	for (;;)
	{
		bzero(buff, MAX);

		// read the message from client and copy it in buffer
		read(connfd, buff, sizeof(buff));
		// print buffer which contains the client contents

		// signal handler.
		if (strncmp("QUIT", buff, 4) == 0)
		{
			printf("Client Exited...\nWaiting for new\n");
			break;
		}
		else if (strncmp("PORT", buff, 4) == 0)
		{
			port(connfd);
		}
		else if (strncmp("STOR", buff, 4) == 0)
		{
			stor(connfd);
		}
		else if (strncmp("RETR", buff, 4) == 0)
		{
			retr(connfd);
		}
		else if (strncmp("USER", buff, 4) == 0)
		{
			printf("Connection Successful\n");
			write(connfd, "200\n", 4);
		}
		else if (strncmp("RNFR", buff, 4) == 0)
		{
			rnfr(connfd);
		}
		else if (strncmp("RNTO", buff, 4) == 0)
		{
			rnto(connfd);
		}
		else if (strncmp("DELE", buff, 4) == 0)
		{
			dele(connfd);
		}
		else if (strncmp("RMD", buff, 3) == 0)
		{
			rmd(connfd);
		}
		else if (strncmp("MKD", buff, 3) == 0)
		{
			mkd(connfd);
		}
		else if (strncmp("PWD", buff, 3) == 0)
		{
			pwd(connfd);
		}
		else if (strncmp("LIST", buff, 4) == 0)
		{
			list(connfd);
		}
		else if (strncmp("NOOP", buff, 4) == 0)
		{
			write(connfd, "200\n", 4);
		}
		else if (strncmp("CDUP",buff,4)==0)
		{
			if(chdir("..")==-1)
				write(connfd,"500\n",4);
			else
				write(connfd,"200\n",4);
		}
		else
		{
			write(connfd, "500\n", 4);
		}
	}
}

void signal_recieved()
{
	exit(0);
}

// Driver function
int main(int argc, char *argv)
{
	if (argv == 3)
	{
		// provided directory
		if (chdir(argv[2]) == -1)
			chdir(".");
	}
	else
	{
		// not provided directory continuing with current directory
		chdir(".");
	}
	int sockfd, len;
	struct sockaddr_in servaddr, cli;

	signal(SIGINT, signal_recieved);
	signal(SIGTSTP, signal_recieved);

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
	{
		printf("socket bind failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");

	// Now server is ready to listen and verification
	if ((listen(sockfd, 5)) != 0)
	{
		printf("Listen failed...\n");
		exit(0);
	}
	else
		printf("Server listening..\n");
	len = sizeof(cli);

	// Accept the data packet from client and verification

	while (1)
	{
		connfd = accept(sockfd, (SA *)&cli, &len);
		if (connfd != -1)
		{
			printf("Client Connected\n");
			func(connfd);
		}
	}
}
