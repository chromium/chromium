// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/rand_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sstream>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/files/file_util.h"
#include "base/allocator/partition_allocator/partition_alloc_base/no_destructor.h"
#include "base/allocator/partition_allocator/partition_alloc_base/posix/eintr_wrapper.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "build/build_config.h"

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(IS_NACL)
#include "third_party/lss/linux_syscall_support.h"
#elif BUILDFLAG(IS_MAC)
// TODO(crbug.com/995996): Waiting for this header to appear in the iOS SDK.
// (See below.)
#include <sys/random.h>
#endif

namespace {

#if BUILDFLAG(IS_AIX)
// AIX has no 64-bit support for O_CLOEXEC.
static constexpr int kOpenFlags = O_RDONLY;
#else
static constexpr int kOpenFlags = O_RDONLY | O_CLOEXEC;
#endif

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use a static-local variable to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd() : fd_(PA_HANDLE_EINTR(open("/dev/urandom", kOpenFlags))) {
    PA_CHECK(fd_ >= 0) << "Cannot open /dev/urandom";
  }

  ~URandomFd() { close(fd_); }

  int fd() const { return fd_; }

 private:
  const int fd_;
};

int GetUrandomFD() {
  static partition_alloc::internal::base::NoDestructor<URandomFd> urandom_fd;
  return urandom_fd->fd();
}

}  // namespace

namespace partition_alloc::internal::base {

// NOTE: In an ideal future, all implementations of this function will just
// wrap BoringSSL's `RAND_bytes`. TODO(crbug.com/995996): Figure out the
// build/test/performance issues with dcheng's CL
// (https://chromium-review.googlesource.com/c/chromium/src/+/1545096) and land
// it or some form of it.
void RandBytes(void* output, size_t output_length) {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(IS_NACL)
  // We have to call `getrandom` via Linux Syscall Support, rather than through
  // the libc wrapper, because we might not have an up-to-date libc (e.g. on
  // some bots).
  const ssize_t r = PA_HANDLE_EINTR(sys_getrandom(output, output_length, 0));

  // Return success only on total success. In case errno == ENOSYS (or any other
  // error), we'll fall through to reading from urandom below.
  if (output_length == static_cast<size_t>(r)) {
    PA_MSAN_UNPOISON(output, output_length);
    return;
  }
#elif BUILDFLAG(IS_MAC)
  // TODO(crbug.com/995996): Enable this on iOS too, when sys/random.h arrives
  // in its SDK.
  if (getentropy(output, output_length) == 0) {
    return;
  }
#endif

  // If the OS-specific mechanisms didn't work, fall through to reading from
  // urandom.
  //
  // TODO(crbug.com/995996): When we no longer need to support old Linux
  // kernels, we can get rid of this /dev/urandom branch altogether.
  const int urandom_fd = GetUrandomFD();
  const bool success =
      ReadFromFD(urandom_fd, static_cast<char*>(output), output_length);
  PA_CHECK(success);
}

}  // namespace partition_alloc::internal::base
