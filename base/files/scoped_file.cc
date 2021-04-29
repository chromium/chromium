// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_file.h"

#include "base/check.h"
#include "build/build_config.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <errno.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

#include <dlfcn.h>

static void (*gRecordReplayAssertFn)(const char*, va_list);

static void RecordReplayAssert(const char* aFormat, ...) {
  if (!gRecordReplayAssertFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayAssert");
    if (!fnptr) {
      return;
    }
    gRecordReplayAssertFn = reinterpret_cast<void(*)(const char*, va_list)>(fnptr);
  }

  va_list ap;
  va_start(ap, aFormat);
  gRecordReplayAssertFn(aFormat, ap);
  va_end(ap);
}

namespace base {
namespace internal {

#if defined(OS_POSIX) || defined(OS_FUCHSIA)

// static
void ScopedFDCloseTraits::Free(int fd) {
  RecordReplayAssert("ScopedFDCloseTraits::Free %d", fd);

  // It's important to crash here.
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  int ret = IGNORE_EINTR(close(fd));

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_APPLE) || \
    defined(OS_FUCHSIA) || defined(OS_ANDROID)
  // NB: Some file descriptors can return errors from close() e.g. network
  // filesystems such as NFS and Linux input devices. On Linux, macOS, and
  // Fuchsia's POSIX layer, errors from close other than EBADF do not indicate
  // failure to actually close the fd.
  if (ret != 0 && errno != EBADF)
    ret = 0;
#endif

  PCHECK(0 == ret);
}

#endif  // OS_POSIX || OS_FUCHSIA

}  // namespace internal
}  // namespace base
