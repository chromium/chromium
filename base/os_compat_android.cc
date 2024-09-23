// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/os_compat_android.h"

#include <array>

#include <asm/unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

extern "C" {
#if __ANDROID_API__ < 26
int futimes(int fd, const struct timeval tv_ptr[2]) {
  if (tv_ptr == nullptr) {
    return base::checked_cast<int>(syscall(__NR_utimensat, fd, NULL, NULL, 0));
  }

  // SAFETY: The caller is required to give an array of two elements.
  auto tv = UNSAFE_BUFFERS(base::span(tv_ptr, 2u));
  if (tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 ||
      tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000) {
    errno = EINVAL;
    return -1;
  }

  // Convert timeval to timespec.
  std::array<struct timespec, 2> ts;
  ts[0].tv_sec = tv[0].tv_sec;
  ts[0].tv_nsec = tv[0].tv_usec * 1000;
  ts[1].tv_sec = tv[1].tv_sec;
  ts[1].tv_nsec = tv[1].tv_usec * 1000;
  return base::checked_cast<int>(
      syscall(__NR_utimensat, fd, NULL, ts.data(), 0));
}
#endif

}  // extern "C"
