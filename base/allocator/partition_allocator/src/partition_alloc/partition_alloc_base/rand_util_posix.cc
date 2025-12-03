// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/rand_util.h"

#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <sstream>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/files/file_util.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"

#if PA_BUILDFLAG(IS_MAC)
// TODO(crbug.com/40641285): Waiting for this header to appear in the iOS SDK.
// (See below.)
#include <sys/random.h>
#endif

namespace {

#if PA_BUILDFLAG(IS_AIX)
// AIX has no 64-bit support for O_CLOEXEC.
static constexpr int kOpenFlags = O_RDONLY;
#else
static constexpr int kOpenFlags = O_RDONLY | O_CLOEXEC;
#endif

// On Android the 'open' function has two versions:
// int open(const char *pathname, int flags);
// int open(const char *pathname, int flags, mode_t mode);
//
// This doesn't play well with WrapEINTR template. This alias helps the compiler
// to make a decision.
int OpenFile(const char* pathname, int flags) {
  return open(pathname, flags);
}

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use a static-local variable to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd()
      : fd_(partition_alloc::WrapEINTR(OpenFile)("/dev/urandom", kOpenFlags)) {
    PA_BASE_CHECK(fd_ >= 0) << "Cannot open /dev/urandom";
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
// wrap BoringSSL's `RAND_bytes`. TODO(crbug.com/40641285): Figure out the
// build/test/performance issues with dcheng's CL
// (https://chromium-review.googlesource.com/c/chromium/src/+/1545096) and land
// it or some form of it.
void RandBytes(void* output, size_t output_length) {
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  // Use `syscall(__NR_getrandom...` to avoid a dependency on
  // `third_party/linux_syscall_support.h`.
  //
  // Here in PartitionAlloc, we don't need to look before we leap
  // because we know that both Linux and CrOS only support kernels
  // that do have this syscall defined. This diverges from upstream
  // `//base` behavior both here and below.
  const ssize_t r =
      WrapEINTR(syscall)(__NR_getrandom, output, output_length, 0);

  // Return success only on total success. In case errno == ENOSYS (or any other
  // error), we'll fall through to reading from urandom below.
  if (output_length == static_cast<size_t>(r)) {
    PA_MSAN_UNPOISON(output, output_length);
    return;
  }
#elif PA_BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40641285): Enable this on iOS too, when sys/random.h arrives
  // in its SDK.
  if (getentropy(output, output_length) == 0) {
    return;
  }
#endif
  // If getrandom(2) above returned with an error and the /dev/urandom fallback
  // took place on Linux/ChromeOS bots, they would fail with a CHECK in
  // nacl_helper. The latter assumes that the number of open file descriptors
  // must be constant. The nacl_helper knows about the FD from
  // //base/rand_utils, but is not aware of the urandom_fd from this file (see
  // CheckForExpectedNumberOfOpenFds).
  //
  // *  On `linux_chromium_asan_rel_ng` in
  //    `ContentBrowserTest.RendererCrashCallStack`:
  //    ```
  //    [FATAL:rand_util_posix.cc(45)] Check failed: fd_ >= 0. Cannot open
  //    /dev/urandom
  //    ```
  // *  On `linux-lacros-rel` in
  //    `NaClBrowserTestGLibc.CrashInCallback`:
  //    ```
  //    2023-07-03T11:31:13.115755Z FATAL nacl_helper:
  //    [nacl_sandbox_linux.cc(178)] Check failed: expected_num_fds ==
  //    sandbox::ProcUtil::CountOpenFds(proc_fd_.get()) (6 vs. 7)
  //    ```
  const int urandom_fd = GetUrandomFD();
  const bool success =
      ReadFromFD(urandom_fd, static_cast<char*>(output), output_length);
  PA_BASE_CHECK(success);
}

}  // namespace partition_alloc::internal::base
