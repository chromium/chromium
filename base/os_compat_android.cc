// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/os_compat_android.h"

#include <asm/unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/numerics/safe_conversions.h"

extern "C" {
#if __ANDROID_API__ < 26
int futimes(int fd, const struct timeval tv[2]) {
  if (tv == nullptr)
    return base::checked_cast<int>(syscall(__NR_utimensat, fd, NULL, NULL, 0));

  if (tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 ||
      tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000) {
    errno = EINVAL;
    return -1;
  }

  // Convert timeval to timespec.
  struct timespec ts[2];
  ts[0].tv_sec = tv[0].tv_sec;
  ts[0].tv_nsec = tv[0].tv_usec * 1000;
  ts[1].tv_sec = tv[1].tv_sec;
  ts[1].tv_nsec = tv[1].tv_usec * 1000;
  return base::checked_cast<int>(syscall(__NR_utimensat, fd, NULL, ts, 0));
}
#endif

}  // extern "C"
