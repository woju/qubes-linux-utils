/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "libqrexec-utils.h"

static do_exec_t *exec_func = NULL;
void register_exec_func(do_exec_t *func) {
	exec_func = func;
}

void fix_fds(int fdin, int fdout, int fderr)
{
	int i;
	for (i = 0; i < 256; i++)
		if (i != fdin && i != fdout && i != fderr)
			close(i);
	dup2(fdin, 0);
	dup2(fdout, 1);
	dup2(fderr, 2);
	close(fdin);
	close(fdout);
	if (fderr != 2)
		close(fderr);
}

void do_fork_exec(const char *cmdline, int *pid, int *stdin_fd, int *stdout_fd,
		  int *stderr_fd)
{
	int inpipe[2], outpipe[2], errpipe[2];

	if (!stdout_fd) {
		/* when no stdout_fd requested, use one socket for both stdin and stdout */
		/* currently used only by qrexec-agent */
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, inpipe) || (stderr_fd && socketpair(AF_UNIX, SOCK_STREAM, 0, errpipe))) {
			perror("socketpair");
			exit(1);
		}
		outpipe[0] = inpipe[1];
		outpipe[1] = inpipe[0];
	} else {
		/* left for compatibility and for qrexec-daemon */
		if (pipe(inpipe) || pipe(outpipe) || (stderr_fd && pipe(errpipe))) {
			perror("pipe");
			exit(1);
		}
	}
	switch (*pid = fork()) {
	case -1:
		perror("fork");
		exit(-1);
	case 0:
		if (stderr_fd) {
			fix_fds(inpipe[0], outpipe[1], errpipe[1]);
		} else
			fix_fds(inpipe[0], outpipe[1], 2);

		if (exec_func != NULL)
			exec_func(cmdline);
		exit(-1);
	default:;
	}
	close(inpipe[0]);
	*stdin_fd = inpipe[1];
	if (stdout_fd) {
		close(outpipe[1]);
		*stdout_fd = outpipe[0];
	}
	if (stderr_fd) {
		close(errpipe[1]);
		*stderr_fd = errpipe[0];
	}
}
