// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>

#include <cstddef>

#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

namespace partition_alloc::internal::base {

void InitThreading() {}

void TerminateOnThread() {
  // PartitionAlloc tests don't use AttachCurrentThread(), because
  // the tests don't set / get any thread priority. So no need to do
  // "base::android::DetachFromVM();" here.
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(ADDRESS_SANITIZER)
  return 0;
#else
  // AddressSanitizer bloats the stack approximately 2x. Default stack size of
  // 1Mb is not enough for some tests (see http://crbug.com/263749 for example).
  return 2 * (1 << 20);  // 2Mb
#endif
}

}  // namespace partition_alloc::internal::base
