// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_switch.h>
#include <sys/resource.h>

#include <algorithm>
#include <atomic>
#include <cstddef>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

namespace partition_alloc::internal::base {

// If Foundation is to be used on more than one thread, it must know that the
// application is multithreaded.  Since it's possible to enter Foundation code
// from threads created by pthread_thread_create, Foundation won't necessarily
// be aware that the application is multithreaded.  Spawning an NSThread is
// enough to get Foundation to set up for multithreaded operation, so this is
// done if necessary before pthread_thread_create spawns any threads.
//
// https://developer.apple.com/documentation/foundation/nsthread/1410702-ismultithreaded
void InitThreading() {
  static BOOL multithreaded = [NSThread isMultiThreaded];
  if (!multithreaded) {
    // +[NSObject class] is idempotent.
    [NSThread detachNewThreadSelector:@selector(class)
                             toTarget:[NSObject class]
                           withObject:nil];
    multithreaded = YES;

    PA_BASE_DCHECK([NSThread isMultiThreaded]);
  }
}

// static
void PlatformThreadForTesting::YieldCurrentThread() {
  // Don't use sched_yield(), as it can lead to 10ms delays.
  //
  // This only depresses the thread priority for 1ms, which is more in line
  // with what calling code likely wants. See this bug in webkit for context:
  // https://bugs.webkit.org/show_bug.cgi?id=204871
  mach_msg_timeout_t timeout_ms = 1;
  thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeout_ms);
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if PA_BUILDFLAG(IS_IOS)
  return 0;
#else
  // The macOS default for a pthread stack size is 512kB.
  // Libc-594.1.4/pthreads/pthread.c's pthread_attr_init uses
  // DEFAULT_STACK_SIZE for this purpose.
  //
  // 512kB isn't quite generous enough for some deeply recursive threads that
  // otherwise request the default stack size by specifying 0. Here, adopt
  // glibc's behavior as on Linux, which is to use the current stack size
  // limit (ulimit -s) as the default stack size. See
  // glibc-2.11.1/nptl/nptl-init.c's __pthread_initialize_minimal_internal. To
  // avoid setting the limit below the macOS default or the minimum usable
  // stack size, these values are also considered. If any of these values
  // can't be determined, or if stack size is unlimited (ulimit -s unlimited),
  // stack_size is left at 0 to get the system default.
  //
  // macOS normally only applies ulimit -s to the main thread stack. On
  // contemporary macOS and Linux systems alike, this value is generally 8MB
  // or in that neighborhood.
  size_t default_stack_size = 0;
  struct rlimit stack_rlimit;
  if (pthread_attr_getstacksize(&attributes, &default_stack_size) == 0 &&
      getrlimit(RLIMIT_STACK, &stack_rlimit) == 0 &&
      stack_rlimit.rlim_cur != RLIM_INFINITY) {
    default_stack_size = std::max(
        std::max(default_stack_size, static_cast<size_t>(PTHREAD_STACK_MIN)),
        static_cast<size_t>(stack_rlimit.rlim_cur));
  }
  return default_stack_size;
#endif
}

void TerminateOnThread() {}

}  // namespace partition_alloc::internal::base
