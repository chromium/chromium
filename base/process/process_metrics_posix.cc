// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include "base/allocator/buildflags.h"
#include "base/logging.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include <sys/resource.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <features.h>

#include "base/numerics/safe_conversions.h"
#endif

namespace base {

int64_t TimeValToMicroseconds(const struct timeval& tv) {
  int64_t ret = tv.tv_sec;  // Avoid (int * int) integer overflow.
  ret *= Time::kMicrosecondsPerSecond;
  ret += tv.tv_usec;
  return ret;
}

#if !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
static const rlim_t kSystemDefaultMaxFds = 8192;
#elif BUILDFLAG(IS_APPLE)
static const rlim_t kSystemDefaultMaxFds = 256;
#elif BUILDFLAG(IS_SOLARIS)
static const rlim_t kSystemDefaultMaxFds = 8192;
#elif BUILDFLAG(IS_FREEBSD)
static const rlim_t kSystemDefaultMaxFds = 8192;
#elif BUILDFLAG(IS_NETBSD)
static const rlim_t kSystemDefaultMaxFds = 1024;
#elif BUILDFLAG(IS_OPENBSD)
static const rlim_t kSystemDefaultMaxFds = 256;
#elif BUILDFLAG(IS_ANDROID)
static const rlim_t kSystemDefaultMaxFds = 1024;
#elif BUILDFLAG(IS_AIX)
static const rlim_t kSystemDefaultMaxFds = 8192;
#endif

size_t GetMaxFds() {
  rlim_t max_fds;
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // getrlimit failed. Take a best guess.
    max_fds = kSystemDefaultMaxFds;
    RAW_LOG(ERROR, "getrlimit(RLIMIT_NOFILE) failed");
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  return static_cast<size_t>(max_fds);
}

size_t GetHandleLimit() {
#if BUILDFLAG(IS_APPLE)
  // Taken from a small test that allocated ports in a loop.
  return static_cast<size_t>(1 << 18);
#else
  return GetMaxFds();
#endif
}

void IncreaseFdLimitTo(unsigned int max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    rlim_t new_limit = max_descriptors;
    if (max_descriptors <= limits.rlim_cur)
      return;
    if (limits.rlim_max > 0 && limits.rlim_max < max_descriptors) {
      new_limit = limits.rlim_max;
    }
    limits.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      PLOG(INFO) << "Failed to set file descriptor limit";
    }
  } else {
    PLOG(INFO) << "Failed to get file descriptor limit";
  }
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
namespace {

size_t GetMallocUsageMallinfo() {
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
#define MALLINFO2_FOUND_IN_LIBC
  struct mallinfo2 minfo = mallinfo2();
#endif
#endif  // defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if !defined(MALLINFO2_FOUND_IN_LIBC)
  struct mallinfo minfo = mallinfo();
#endif
#undef MALLINFO2_FOUND_IN_LIBC
  return checked_cast<size_t>(minfo.hblkhd + minfo.arena);
}

}  // namespace
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

size_t ProcessMetrics::GetMallocUsage() {
#if BUILDFLAG(IS_APPLE)
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);
  return stats.size_in_use;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return GetMallocUsageMallinfo();
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(fuchsia): Not currently exposed. https://crbug.com/735087.
  return 0;
#endif
}

}  // namespace base
