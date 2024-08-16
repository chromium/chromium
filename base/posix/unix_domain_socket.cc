// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/posix/unix_domain_socket.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include <vector>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

namespace base {

const size_t UnixDomainSocket::kMaxFileDescriptors = 16;

bool CreateSocketPair(ScopedFD* one, ScopedFD* two) {
  int raw_socks[2];
#if BUILDFLAG(IS_APPLE)
  // macOS does not support SEQPACKET.
  const int flags = SOCK_STREAM;
#else
  const int flags = SOCK_SEQPACKET;
#endif
  if (socketpair(AF_UNIX, flags, 0, raw_socks) == -1)
    return false;
#if BUILDFLAG(IS_APPLE)
  // On macOS, preventing SIGPIPE is done with socket option.
  const int no_sigpipe = 1;
  if (setsockopt(raw_socks[0], SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                 sizeof(no_sigpipe)) != 0)
    return false;
  if (setsockopt(raw_socks[1], SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                 sizeof(no_sigpipe)) != 0)
    return false;
#endif
  one->reset(raw_socks[0]);
  two->reset(raw_socks[1]);
  return true;
}

// static
bool UnixDomainSocket::EnableReceiveProcessId(int fd) {
#if !BUILDFLAG(IS_APPLE)
  const int enable = 1;
  return setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable)) == 0;
#else
  // SO_PASSCRED is not supported on macOS.
  return true;
#endif  // BUILDFLAG(IS_APPLE)
}

// static
bool UnixDomainSocket::SendMsg(int fd,
                               const void* buf,
                               size_t length,
                               const std::vector<int>& fds) {
  struct msghdr msg = {};
  struct iovec iov = {const_cast<void*>(buf), length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char* control_buffer = nullptr;
  if (fds.size()) {
    const size_t control_len = CMSG_SPACE(sizeof(int) * fds.size());
    control_buffer = new char[control_len];

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
#if BUILDFLAG(IS_APPLE)
    msg.msg_controllen = checked_cast<socklen_t>(control_len);
#else
    msg.msg_controllen = control_len;
#endif
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
#if BUILDFLAG(IS_APPLE)
    cmsg->cmsg_len = checked_cast<u_int>(CMSG_LEN(sizeof(int) * fds.size()));
#else
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
#endif
    memcpy(CMSG_DATA(cmsg), &fds[0], sizeof(int) * fds.size());
    msg.msg_controllen = cmsg->cmsg_len;
  }

// Avoid a SIGPIPE if the other end breaks the connection.
// Due to a bug in the Linux kernel (net/unix/af_unix.c) MSG_NOSIGNAL isn't
// regarded for SOCK_SEQPACKET in the AF_UNIX domain, but it is mandated by
// POSIX. On Mac MSG_NOSIGNAL is not supported, so we need to ensure that
// SO_NOSIGPIPE is set during socket creation.
#if BUILDFLAG(IS_APPLE)
  const int flags = 0;
  int no_sigpipe = 0;
  socklen_t no_sigpipe_len = sizeof(no_sigpipe);
  DPCHECK(getsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                     &no_sigpipe_len) == 0)
      << "Failed ot get socket option.";
  DCHECK(no_sigpipe) << "SO_NOSIGPIPE not set on the socket.";
#else
  const int flags = MSG_NOSIGNAL;
#endif  // BUILDFLAG(IS_APPLE)
  const ssize_t r = HANDLE_EINTR(sendmsg(fd, &msg, flags));
  const bool ret = static_cast<ssize_t>(length) == r;
  delete[] control_buffer;
  return ret;
}

// static
ssize_t UnixDomainSocket::RecvMsg(int fd,
                                  void* buf,
                                  size_t length,
                                  std::vector<ScopedFD>* fds) {
  return UnixDomainSocket::RecvMsgWithPid(fd, buf, length, fds, nullptr);
}

// static
ssize_t UnixDomainSocket::RecvMsgWithPid(int fd,
                                         void* buf,
                                         size_t length,
                                         std::vector<ScopedFD>* fds,
                                         ProcessId* pid) {
  return UnixDomainSocket::RecvMsgWithFlags(fd, buf, length, 0, fds, pid);
}

// static
ssize_t UnixDomainSocket::RecvMsgWithFlags(int fd,
                                           void* buf,
                                           size_t length,
                                           int flags,
                                           std::vector<ScopedFD>* fds,
                                           ProcessId* out_pid) {
  fds->clear();

  struct msghdr msg = {};
  struct iovec iov = {buf, length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const size_t kControlBufferSize =
      CMSG_SPACE(sizeof(int) * kMaxFileDescriptors)
#if !BUILDFLAG(IS_APPLE)
      // macOS does not support ucred.
      // macOS supports xucred, but this structure is insufficient.
      + CMSG_SPACE(sizeof(struct ucred))
#endif  // !BUILDFLAG(IS_APPLE)
      ;
  char control_buffer[kControlBufferSize];
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const ssize_t r = HANDLE_EINTR(recvmsg(fd, &msg, flags));
  if (r == -1)
    return -1;

  int* wire_fds = nullptr;
  size_t wire_fds_len = 0;
  ProcessId pid = -1;

  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        DCHECK_EQ(payload_len % sizeof(int), 0u);
        DCHECK_EQ(wire_fds, static_cast<void*>(nullptr));
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = payload_len / sizeof(int);
      }
#if !BUILDFLAG(IS_APPLE)
      // macOS does not support SCM_CREDENTIALS.
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_CREDENTIALS) {
        DCHECK_EQ(payload_len, sizeof(struct ucred));
        DCHECK_EQ(pid, -1);
        pid = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg))->pid;
      }
#endif  // !BUILDFLAG(IS_APPLE)
    }
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    if (msg.msg_flags & MSG_CTRUNC) {
      // Extraordinary case, not caller fixable. Log something.
      LOG(ERROR) << "recvmsg returned MSG_CTRUNC flag, buffer len is "
                 << msg.msg_controllen;
    }
    for (size_t i = 0; i < wire_fds_len; ++i)
      close(wire_fds[i]);
    errno = EMSGSIZE;
    return -1;
  }

  if (wire_fds) {
    for (size_t i = 0; i < wire_fds_len; ++i)
      fds->push_back(ScopedFD(wire_fds[i]));  // TODO(mdempsky): emplace_back
  }

  if (out_pid) {
#if BUILDFLAG(IS_APPLE)
    socklen_t pid_size = sizeof(pid);
    if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERPID, &pid, &pid_size) != 0)
      pid = -1;
#else
    // |pid| will legitimately be -1 if we read EOF, so only DCHECK if we
    // actually received a message.  Unfortunately, Linux allows sending zero
    // length messages, which are indistinguishable from EOF, so this check
    // has false negatives.
    if (r > 0 || msg.msg_controllen > 0)
      DCHECK_GE(pid, 0);
#endif

    *out_pid = pid;
  }

  return r;
}

// static
ssize_t UnixDomainSocket::SendRecvMsg(int fd,
                                      uint8_t* reply,
                                      unsigned max_reply_len,
                                      int* result_fd,
                                      const Pickle& request) {
  return UnixDomainSocket::SendRecvMsgWithFlags(fd, reply, max_reply_len,
                                                0, /* recvmsg_flags */
                                                result_fd, request);
}

// static
ssize_t UnixDomainSocket::SendRecvMsgWithFlags(int fd,
                                               uint8_t* reply,
                                               unsigned max_reply_len,
                                               int recvmsg_flags,
                                               int* result_fd,
                                               const Pickle& request) {
  // This socketpair is only used for the IPC and is cleaned up before
  // returning.
  ScopedFD recv_sock, send_sock;
  if (!CreateSocketPair(&recv_sock, &send_sock))
    return -1;

  {
    std::vector<int> send_fds;
    send_fds.push_back(send_sock.get());
    if (!SendMsg(fd, request.data(), request.size(), send_fds))
      return -1;
  }

  // Close the sending end of the socket right away so that if our peer closes
  // it before sending a response (e.g., from exiting), RecvMsgWithFlags() will
  // return EOF instead of hanging.
  send_sock.reset();

  std::vector<ScopedFD> recv_fds;
  // When porting to OSX keep in mind it doesn't support MSG_NOSIGNAL, so the
  // sender might get a SIGPIPE.
  const ssize_t reply_len = RecvMsgWithFlags(
      recv_sock.get(), reply, max_reply_len, recvmsg_flags, &recv_fds, nullptr);
  recv_sock.reset();
  if (reply_len == -1)
    return -1;

  // If we received more file descriptors than caller expected, then we treat
  // that as an error.
  if (recv_fds.size() > (result_fd != nullptr ? 1 : 0)) {
    NOTREACHED();
  }

  if (result_fd)
    *result_fd = recv_fds.empty() ? -1 : recv_fds[0].release();

  return reply_len;
}

}  // namespace base
