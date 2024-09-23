// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_file.h"

#include "base/check.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <errno.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace base::internal {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// static
void ScopedFDCloseTraits::Free(int fd) {
  // It's important to crash here if something goes wrong.
  //
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  //
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  int ret = IGNORE_EINTR(close(fd));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
  // NB: Some file descriptors can return errors from close() e.g. network
  // filesystems such as NFS and Linux input devices. On Linux, macOS, and
  // Fuchsia's POSIX layer, errors from close other than EBADF do not indicate
  // failure to actually close the fd.
  if (ret != 0 && errno != EBADF) {
    ret = 0;
  }
#endif

  PCHECK(0 == ret);
}

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

}  // namespace base::internal
