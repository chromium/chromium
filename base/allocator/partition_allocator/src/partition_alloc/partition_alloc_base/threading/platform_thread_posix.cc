// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_internal_posix.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
#include <sys/syscall.h>
#include <atomic>
#endif

#if PA_BUILDFLAG(IS_FUCHSIA)
#include <zircon/process.h>
#endif

namespace partition_alloc::internal::base {

// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if PA_BUILDFLAG(IS_APPLE)
  return pthread_mach_thread_np(pthread_self());
#elif PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  // Unlike base/threading/platform_thread_posix.cc, we avoid using thread_local
  // for thread_id caching. This prevents reentrancy issues within
  // PartitionAlloc, as reported in crbug.com/476192650. Since
  // partition_alloc::internal::base::PlatformThread::CurrentId is called
  // infrequently (typically just a few times per thread), the performance
  // overhead of not using thread_local is acceptable and outweighed by avoiding
  // the bug. If necessary we could cache it but we'd have to ensure it was
  // while we were inside PartitionAlloc itself for allocation already.
  return syscall(__NR_gettid);
#elif PA_BUILDFLAG(IS_ANDROID)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif PA_BUILDFLAG(IS_FUCHSIA)
  return zx_thread_self();
#elif PA_BUILDFLAG(IS_ASMJS)
  return pthread_self();
#elif PA_BUILDFLAG(IS_SOLARIS) || PA_BUILDFLAG(IS_QNX)
  return pthread_self();
#elif PA_BUILDFLAG(IS_POSIX) && PA_BUILDFLAG(IS_AIX)
  return pthread_self();
#elif PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_AIX)
  return reinterpret_cast<int64_t>(pthread_self());
#endif
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(pthread_self());
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  struct timespec sleep_time, remaining;

  // Break the duration into seconds and nanoseconds.
  // NOTE: TimeDelta's microseconds are int64s while timespec's
  // nanoseconds are longs, so this unpacking must prevent overflow.
  sleep_time.tv_sec = duration.InSeconds();
  duration -= Seconds(sleep_time.tv_sec);
  sleep_time.tv_nsec = duration.InMicroseconds() * 1000;  // nanoseconds

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR) {
    sleep_time = remaining;
  }
}

}  // namespace partition_alloc::internal::base
