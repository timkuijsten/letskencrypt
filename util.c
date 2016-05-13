#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static	volatile sig_atomic_t sig;

static	const char *const comms[COMM__MAX] = {
	"req", /* COMM_REQ */
	"thumbprint", /* COMM_THUMB */
	"certficate", /* COMM_CERT */
	"payload", /* COMM_PAY */
	"nonce", /* COMM_NONCE */
	"token", /* COMM_TOK */
	"challenge", /* COMM_CHNG */
	"challenge-ack", /* COMM_CHNG_ACK */
	"challenge-fin", /* COMM_CHNG_FIN */
	"account", /* COMM_SIGN */
};

static void
sigpipe(int code)
{

	(void)code;
	sig = 1;
}

/*
 * This will read a long-sized operation.
 * Operations are usually enums, so this should be alright.
 * We return 0 on EOF and LONG_MAX on failure.
 */
long
readop(const char *sub, int fd, enum comm comm)
{
	ssize_t	 	 ssz;
	long		 op;

	ssz = read(fd, &op, sizeof(long));
	if (ssz < 0) {
		doxwarn(sub, "read: %s", comms[comm]);
		return(LONG_MAX);
	} else if (ssz && ssz != sizeof(long)) {
		doxwarnx(sub, "short read: %s", comms[comm]);
		return(LONG_MAX);
	} else if (0 == ssz)
		return(0);

	return(op);
}

char *
readstr(const char *sub, int fd, enum comm comm)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		*p;

	p = NULL;

	if ((ssz = read(fd, &sz, sizeof(size_t))) < 0)
		doxwarn(sub, "read: %s length", comms[comm]);
	else if ((size_t)ssz != sizeof(size_t))
		doxwarnx(sub, "short read: %s length", comms[comm]);
	else if (NULL == (p = calloc(1, sz + 1)))
		doxwarn(sub, "malloc");
	else if ((ssz = read(fd, p, sz)) < 0)
		doxwarn(sub, "read: %s", comms[comm]);
	else if ((size_t)ssz != sz)
		doxwarnx(sub, "short read: %s", comms[comm]);
	else
		return(p);

	free(p);
	return(NULL);
}

/*
 * Wring a long-value to a communication pipe.
 * Returns zero if the write failed or the pipe is not open, otherwise
 * return non-zero.
 */
int
writeop(const char *sub, int fd, enum comm comm, long op)
{
	ssize_t	 ssz;
	sig_t	 sig;
	int	 rc;

	rc = 0;
	/* Catch a closed pipe. */
	sig = signal(SIGPIPE, sigpipe);

	if ((ssz = write(fd, &op, sizeof(long))) < 0) 
		doxwarn(sub, "write: %s", comms[comm]);
	else if ((size_t)ssz != sizeof(long))
		doxwarnx(sub, "short write: %s", comms[comm]);
	else
		rc = 1;

	/* Reinstate signal handler. */
	signal(SIGPIPE, sig);
	sig = 0;
	return(rc);
}

int
writestr(const char *sub, int fd, enum comm comm, const char *v)
{
	size_t	 sz;
	ssize_t	 ssz;
	int	 rc;
	sig_t	 sig;

	sz = strlen(v);
	rc = 0;
	/* Catch a closed pipe. */
	sig = signal(SIGPIPE, sigpipe);

	if ((ssz = write(fd, &sz, sizeof(size_t))) < 0) 
		doxwarn(sub, "write: %s length", comms[comm]);
	else if ((size_t)ssz != sizeof(size_t))
		doxwarnx(sub, "short write: %s length", comms[comm]);
	else if ((ssz = write(fd, v, sz)) < 0)
		doxwarn(sub, "write: %s", comms[comm]);
	else if ((size_t)ssz != sz)
		doxwarnx(sub, "short write: %s", comms[comm]);
	else
		rc = 1;

	/* Reinstate signal handler. */
	signal(SIGPIPE, sig);
	sig = 0;
	return(rc);
}

char *
readstream(const char *sub, int fd, enum comm comm)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		 buf[BUFSIZ];
	void		*pp;
	char		*p;

	p = NULL;
	sz = 0;
	while ((ssz = read(fd, buf, sizeof(buf))) > 0) {
		if (NULL == (pp = realloc(p, sz + ssz + 1))) {
			doxwarn(sub, "realloc");
			free(p);
			return(NULL);
		}
		p = pp;
		memcpy(p + sz, buf, ssz);
		sz += ssz;
		p[sz] = '\0';
	}

	if (ssz < 0) {
		doxwarn(sub, "read: %s", comms[comm]);
		free(p);
		return(NULL);
	} else if (0 == sz) {
		doxwarnx(sub, "empty read: %s", comms[comm]);
		return(NULL);
	}

	return(p);
}