// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>

#include <cstddef>

#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

namespace partition_alloc::internal::base {

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(THREAD_SANITIZER)
  return 0;
#else   // defined(THREAD_SANITIZER)
  // ThreadSanitizer bloats the stack heavily. Evidence has been that the
  // default stack size isn't enough for some browser tests.
  return 2 * (1 << 23);  // 2 times 8192K (the default stack size on Linux).
#endif  // defined(THREAD_SANITIZER)
}

}  // namespace partition_alloc::internal::base
