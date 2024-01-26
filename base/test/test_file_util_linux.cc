// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <asm/unistd.h>
#include <errno.h>
#include <linux/fadvise.h>
#include <sys/syscall.h>
#endif

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/notimplemented.h"

namespace base {

// Inconveniently, the NDK doesn't provide for posix_fadvise
// until native API level = 21, which we don't use yet, so provide a wrapper, at
// least on ARM32
#if BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21

namespace {
int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
#if defined(ARCH_CPU_ARMEL)
  // Note that the syscall argument order on ARM is different from the C
  // function; this is helpfully documented in the Linux posix_fadvise manpage.
  return syscall(__NR_arm_fadvise64_64, fd, advice,
                 0,  // Upper 32-bits for offset
                 offset,
                 0,  // Upper 32-bits for length
                 len);
#endif
  NOTIMPLEMENTED();
  return ENOSYS;
}

}  // namespace

#endif  // BUILDFLAG(IS_ANDROID)

bool EvictFileFromSystemCache(const FilePath& file) {
  ScopedFD fd(open(file.value().c_str(), O_RDONLY));
  if (!fd.is_valid())
    return false;
  if (fdatasync(fd.get()) != 0)
    return false;
  if (posix_fadvise(fd.get(), 0, 0, POSIX_FADV_DONTNEED) != 0)
    return false;
  return true;
}

}  // namespace base
