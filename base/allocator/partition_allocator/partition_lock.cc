// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_lock.h"

#include "base/allocator/partition_allocator/yield_processor.h"
#include "base/threading/platform_thread.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <sched.h>
#endif

// The YIELD_THREAD macro tells the OS to relinquish our quantum. This is
// basically a worst-case fallback, and if you're hitting it with any frequency
// you really should be using a proper lock (such as |base::Lock|)rather than
// these spinlocks.
#if defined(OS_WIN)
#define YIELD_THREAD SwitchToThread()
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#define YIELD_THREAD sched_yield()

#else  // Other OS

#warning "Thread yield not supported on this OS."
#define YIELD_THREAD ((void)0)

#endif  // OS_WIN

namespace base {
namespace internal {

void SpinLock::AcquireSlow() {
  // The value of |kYieldProcessorTries| is cargo culted from TCMalloc, Windows
  // critical section defaults, and various other recommendations.
  static const int kYieldProcessorTries = 1000;
  // The value of |kYieldThreadTries| is completely made up.
  static const int kYieldThreadTries = 10;
  int yield_thread_count = 0;
  do {
    do {
      for (int count = 0; count < kYieldProcessorTries; ++count) {
        // Let the processor know we're spinning.
        YIELD_PROCESSOR;
        if (!lock_.load(std::memory_order_relaxed) &&
            LIKELY(!lock_.exchange(true, std::memory_order_acquire)))
          return;
      }

      if (yield_thread_count < kYieldThreadTries) {
        ++yield_thread_count;
        // Give the OS a chance to schedule something on this core.
        YIELD_THREAD;
      } else {
        // At this point, it's likely that the lock is held by a lower priority
        // thread that is unavailable to finish its work because of higher
        // priority threads spinning here. Sleeping should ensure that they make
        // progress.
        PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
      }
    } while (lock_.load(std::memory_order_relaxed));
  } while (UNLIKELY(lock_.exchange(true, std::memory_order_acquire)));
}

}  // namespace internal
}  // namespace base
