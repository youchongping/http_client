#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define   MAX_RECEIVED_BUFF       5000
#define   MAX_REQUEST_BODY_SIZE   512
#define   TRUE 1
#define   FALSE 0	
struct http_server_info
{
  	unsigned char hostname[128];
  	unsigned char path[128];
  	int port;
};

int user_http_url_parse(const char *url,struct http_server_info *user_server)
{
    char url_hostname[128] = "";
    int  url_port = 80;
    int secure = FALSE;
    int  ret = 0;

    if(strncmp(url, "http://",  strlen("http://"))  == 0)
    {
        url += strlen("http://"); // Get rid of the protocol.
        secure = FALSE;
    }
    else if(strncmp(url, "https://", strlen("https://")) == 0)
    {
        url_port = 443;
        secure = TRUE;
        url += strlen("https://"); // Get rid of the protocol.
    }
    else
    {
        printf("[error] URL is not HTTP or HTTPS %s\n", url);
        ret = -1;
        goto exit;
    }

    char * url_path = strchr(url, '/');
    if (url_path == NULL)
    {
        url_path = strchr(url, '\0'); // Pointer to end of string.
    }

    char * url_colon = strchr(url, ':');
    if (url_colon > url_path)
    {
        url_colon = NULL; // Limit the search to characters before the path.
    }

    if (url_colon == NULL)
    {
        memcpy(url_hostname, url, url_path - url);
        url_hostname[url_path - url] = '\0';
    }
    else
    {
        url_port = atoi(url_colon + 1);
        if (url_port == 0)
        {
            printf("[error] Port error %s\n", url);
            ret = -2 ;
            goto exit;
        }
        memcpy(url_hostname, url, url_colon - url);
        url_hostname[url_colon - url] = '\0';
    }

    if (url_path[0] == '\0')   // Empty path is not allowed
    {
        url_path = "/";
    }

    memset(user_server,0,sizeof(struct http_server_info));
    memcpy(user_server->hostname,url_hostname,strlen(url_hostname));
    user_server->port = url_port;
    memcpy(user_server->path,url_path,strlen(url_path));

 exit :
     return ret;
}

void received_buf_process(const char *response_body,ssize_t nread)
{
	//fprintf(stdout,"+++++++++response %d bytes:\n%s \n",(int)nread,response_body);	
	char public_ip_str[20];
	memset(public_ip_str,0,sizeof(public_ip_str));

	char *ip_string_start = strstr(response_body,"<p class=\"ipaddress\">");
	if(ip_string_start != NULL)
	{
		ip_string_start = ip_string_start + strlen("<p class=\"ipaddress\">");

		char*ip_string_end = strstr(ip_string_start,"</p>");
		if((ip_string_end != NULL)&&(ip_string_end - ip_string_start < sizeof(public_ip_str)))
		{
			strncpy(public_ip_str,ip_string_start,ip_string_end - ip_string_start);
			printf("+++++++++ my public ip is:%s \n",public_ip_str);
        	}
    	}
    	else
        	printf("does not find ipstring ");
}

int main(int argc,char** argv)
{

	struct addrinfo hints;
	struct addrinfo *res,*rp;
	int s;
	int sock_fd;
	char* nodes = "http://whatismyip.host/";
	char service[6] = {0};// "http" or "80" is the same ,"https" or "443" is the same
	if (argc < 1) 
	{
		fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
		exit(EXIT_FAILURE);
	 }
  	struct http_server_info user_server_url;
	int ret  =  user_http_url_parse(nodes,&user_server_url);
	if(ret != 0)
	{
		printf("[error] url parse error :  %d",ret);
		exit(EXIT_FAILURE);
	}
	snprintf(service,sizeof(service),"%d",user_server_url.port);
	/*hints init*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;//both IPv4 & IPv6 will returned
	hints.ai_socktype = SOCK_STREAM;//TCP
	hints.ai_protocol = 0;//any protocol can be returned, TCP UDP ICMP etc.
	
	/*The getaddrinfo() function combines the functionality
	provided by the gethostbyname(3) and getservbyname(3) 		functions into  a single  interface */
	s = getaddrinfo(user_server_url.hostname, service, &hints, &res);
	if (s != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
 		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */
	for(rp=res; rp!=NULL; rp=rp->ai_next)
	{
		sock_fd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if(sock_fd == -1)
			continue;
		if(connect(sock_fd,rp->ai_addr,rp->ai_addrlen) != -1)
			break; /*success*/
		close(sock_fd);
	}
	
	if (rp == NULL) 
	{  
		/* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
 	}

	freeaddrinfo(res);           /* No longer needed */


	/*write*/
	char write_buf[MAX_REQUEST_BODY_SIZE] = "";
	size_t len;  
	
	
	memset(write_buf,0,sizeof(write_buf));
	snprintf(write_buf,MAX_REQUEST_BODY_SIZE,"GET %s HTTP/1.1\r\nHost:%s\r\nUser-Agent:xxx\r\nConnection:close \r\n\r\n{}",
                user_server_url.path,user_server_url.hostname);
	len = strlen(write_buf);
	//fprintf(stdout,"+++++++++write %d bytes:\n%s \n\n",(int)len,write_buf);
	if (write(sock_fd, write_buf, len) != len) //send http header
	{
		fprintf(stderr, "partial/failed write\n");
		exit(EXIT_FAILURE);
	}
	
	
	/*read*/
	ssize_t nread=0;
	ssize_t r=0;
	char read_buf[MAX_RECEIVED_BUFF]={0};
	memset(read_buf,0,sizeof(read_buf));
	char cache_buf[128]={0};
	do
	{
		memset(cache_buf,0,sizeof(cache_buf));
  		r = read(sock_fd,cache_buf,sizeof(cache_buf));
		strcat(read_buf,cache_buf);
		nread += r;
	}while(r>0);
	
	received_buf_process(read_buf,nread);
	close(sock_fd);
	exit(EXIT_SUCCESS);
	
}
