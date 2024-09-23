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

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

namespace {

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally.
thread_local pid_t g_thread_id = -1;

// A boolean value that indicates that the value stored in |g_thread_id| on the
// main thread is invalid, because it hasn't been updated since the process
// forked.
//
// This used to work by setting |g_thread_id| to -1 in a pthread_atfork handler.
// However, when a multithreaded process forks, it is only allowed to call
// async-signal-safe functions until it calls an exec() syscall. However,
// accessing TLS may allocate (see crbug.com/1275748), which is not
// async-signal-safe and therefore causes deadlocks, corruption, and crashes.
//
// It's Atomic to placate TSAN.
std::atomic<bool> g_main_thread_tid_cache_valid = false;

// Tracks whether the current thread is the main thread, and therefore whether
// |g_main_thread_tid_cache_valid| is relevant for the current thread. This is
// also updated by PlatformThread::CurrentId().
thread_local bool g_is_main_thread = true;

class InitAtFork {
 public:
  InitAtFork() {
    pthread_atfork(nullptr, nullptr, internal::InvalidateTidCache);
  }
};

}  // namespace

namespace internal {

void InvalidateTidCache() {
  g_main_thread_tid_cache_valid.store(false, std::memory_order_relaxed);
}

}  // namespace internal

#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if PA_BUILDFLAG(IS_APPLE)
  return pthread_mach_thread_np(pthread_self());
#elif PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  static InitAtFork init_at_fork;
  if (g_thread_id == -1 ||
      (g_is_main_thread &&
       !g_main_thread_tid_cache_valid.load(std::memory_order_relaxed))) {
    // Update the cached tid.
    g_thread_id = syscall(__NR_gettid);
    // If this is the main thread, we can mark the tid_cache as valid.
    // Otherwise, stop the current thread from always entering this slow path.
    if (g_thread_id == getpid()) {
      g_main_thread_tid_cache_valid.store(true, std::memory_order_relaxed);
    } else {
      g_is_main_thread = false;
    }
  } else {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    if (g_thread_id != syscall(__NR_gettid)) {
      PA_RAW_LOG(
          FATAL,
          "Thread id stored in TLS is different from thread id returned by "
          "the system. It is likely that the process was forked without going "
          "through fork().");
    }
#endif
  }
  return g_thread_id;
#elif PA_BUILDFLAG(IS_ANDROID)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif PA_BUILDFLAG(IS_FUCHSIA)
  return zx_thread_self();
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
