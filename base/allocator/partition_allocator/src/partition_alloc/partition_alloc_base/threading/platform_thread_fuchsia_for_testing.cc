// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <pthread.h>

#include <cstddef>

#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"

namespace partition_alloc::internal::base {

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

}  // namespace partition_alloc::internal::base
