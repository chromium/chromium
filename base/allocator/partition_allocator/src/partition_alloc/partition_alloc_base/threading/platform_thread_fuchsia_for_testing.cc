// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

#include <pthread.h>
#include <stddef.h>

namespace partition_alloc::internal::base {

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

}  // namespace partition_alloc::internal::base
