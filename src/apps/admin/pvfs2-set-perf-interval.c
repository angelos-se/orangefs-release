/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>

#include "pvfs2.h"
#include "pvfs2-mgmt.h"
#include "pint-cached-config.h"

#ifndef PVFS2_VERSION
#define PVFS2_VERSION "Unknown"
#endif

struct options
{
    char *mnt_point;
    int mnt_point_set;
    int interval;
    int interval_set;
    char *server;
};

static struct options* parse_args(int argc, char *argv[]);
static void usage(int argc, char **argv);

int main(int argc, char **argv)
{
    int ret = -1;
    PVFS_fs_id cur_fs;
    struct options *user_opts = NULL;
    char pvfs_path[PVFS_NAME_MAX] = {0};
    PVFS_credential creds;
    struct PVFS_mgmt_setparam_value param_value;
    int server_type = 0;

    /* look at command line arguments */
    user_opts = parse_args(argc, argv);
    if(!user_opts)
    {
	fprintf(stderr, "Error: failed to parse command line arguments.\n");
	usage(argc, argv);
	return(-1);
    }

    ret = PVFS_util_init_defaults();
    if(ret < 0)
    {
	PVFS_perror("PVFS_util_init_defaults", ret);
	return(-1);
    }

    /* translate local path into pvfs2 relative path */
    ret = PVFS_util_resolve(user_opts->mnt_point,
                            &cur_fs,
                            pvfs_path,
                            PVFS_NAME_MAX);
    if(ret < 0)
    {
	fprintf(stderr,
                "Error: could not find filesystem for %s in pvfstab\n", 
	        user_opts->mnt_point);
	return(-1);
    }

    ret = PVFS_util_gen_credential_defaults(&creds);
    if (ret < 0)
    {
        PVFS_perror("PVFS_util_gen_credential_defaults", ret);
        return(-1);
    }

    param_value.type = PVFS_MGMT_PARAM_TYPE_UINT64;
    param_value.u.value = user_opts->interval;

    if (user_opts->server)
    {
       /* verify server string */
       ret = PINT_cached_config_check_type(cur_fs,
                                           user_opts->server,
                                           &server_type);
       if (ret)
       {
          fprintf(stderr,
                  "Server string (%s) is undefined. Check config file.\n",
                  user_opts->server);
          goto out;
       }
       ret = PVFS_mgmt_setparam_single(cur_fs,
                                       &creds,
                                       PVFS_SERV_PARAM_PERF_INTERVAL,
                                       &param_value,
                                       user_opts->server,    /*server string*/
                                       NULL,                 /*details*/
                                       NULL                  /*hints*/);
       if (ret)
       {
          fprintf(stderr,
                  "Error(%d) setting interval on server(%s)\n",
                  ret,
                  user_opts->server);
          goto out;
       }        
       else
       {
          fprintf(stderr,
                  "Successfully set interval(%d) on server (%s)\n",
                  user_opts->interval,
                  user_opts->server);
       }
    }
    else
    {
        ret = PVFS_mgmt_setparam_all(cur_fs,
	                             &creds,
				     PVFS_SERV_PARAM_PERF_INTERVAL,
                                     &param_value,
				     NULL,/* details */
				     NULL /* hints */);
        if (ret)
        {
           fprintf(stderr,
                   "Error(%d) setting interval for mount point(%s)\n",
                   ret,
                   user_opts->mnt_point);
           goto out;
        }
        else
        {
           fprintf(stderr,
                   "Successfully set interval (%d) for mount point(%s)\n",
                   user_opts->interval,
                   user_opts->mnt_point);
        }
    }

out:
    PVFS_sys_finalize();

    return(ret);
}


/* parse_args()
 *
 * parses command line arguments
 *
 * returns pointer to options structure on success, NULL on failure
 */
static struct options *parse_args(int argc, char* argv[])
{
    char flags[] = "vm:s:";
    int one_opt = 0;
    int len = 0;

    struct options* tmp_opts = NULL;
    int ret = -1;

    /* create storage for the command line options */
    tmp_opts = (struct options *)malloc(sizeof(struct options));
    if(!tmp_opts)
    {
	return(NULL);
    }
    memset(tmp_opts, 0, sizeof(struct options));

    /* look at command line arguments */
    while((one_opt = getopt(argc, argv, flags)) != EOF)
    {
	switch(one_opt)
        {
            case('v'):
                printf("%s\n", PVFS2_VERSION);
                exit(0);
	    case('m'):
		len = strlen(optarg) + 1;
		tmp_opts->mnt_point = (char*)malloc(len + 1);
		if(!tmp_opts->mnt_point)
		{
		    free(tmp_opts);
		    return(NULL);
		}
		memset(tmp_opts->mnt_point, 0, len + 1);
		ret = sscanf(optarg, "%s", tmp_opts->mnt_point);
		if(ret < 1)
                {
		    free(tmp_opts);
		    return(NULL);
		}
		/* TODO: dirty hack... fix later.  The remove_dir_prefix()
		 * function expects some trailing segments or at least
		 * a slash off of the mount point
		 */
		strcat(tmp_opts->mnt_point, "/");
		tmp_opts->mnt_point_set = 1;
		break;
            case('s'):
                tmp_opts->server = strdup(optarg);
                break;
	    case('?'):
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
    }

    if(optind != (argc - 1))
    {
	usage(argc, argv);
	exit(EXIT_FAILURE);
    }

    tmp_opts->interval = atoi(argv[argc - 1]);
    tmp_opts->interval_set = 1;
    if(!tmp_opts->mnt_point_set || !tmp_opts->interval >= 1)
    {
        if(!tmp_opts->mnt_point_set)
        {
            fprintf(stderr,"Error: Mount point is required.\n");
        }
        if(!tmp_opts->interval_set)
        {
            fprintf(stderr,"Error: Interval is required.\n");
        }
        if(!tmp_opts->interval <= 0)
        {
            fprintf(stderr,"Error: Interval is must be greater than 0.\n");
        }
	if(tmp_opts->mnt_point)
        {
	    free(tmp_opts->mnt_point);
        }
	free(tmp_opts);
	return(NULL);
    }

    return(tmp_opts);
}


static void usage(int argc, char** argv)
{
    fprintf(stderr, "\n");
    fprintf(stderr,
            "Usage  : %s [-s server] -m <filesystem mount point>  <interval>\n\n",
	    argv[0]);
    fprintf(stderr,
            "Mount point and interval are required. "
            "If server is given, then interval will be set only on that server; "
            "otherwise, interval is set on "
            "all servers for the given mount point.\n\n");
    fprintf(stderr,
            "Example:All-Severs: %s -m /mnt/pvfs2 6000\n\n",
	    argv[0]);
    fprintf(stderr,
            "Example:One-Server: %s -s tcp://localhost:3334/pvfs2-fs -m /mnt/pvfs2 10000\n\n",argv[0]);
    fprintf(stderr, "Interval is an integer greater than 0 in milliseconds\n");

    return;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */

