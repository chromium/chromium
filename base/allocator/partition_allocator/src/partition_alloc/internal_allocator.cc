// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/internal_allocator.h"

namespace partition_alloc::internal {
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PartitionRoot& InternalAllocatorRoot() {
  static internal::base::NoDestructor<PartitionRoot> allocator([]() {
    // Disable features using the internal root to avoid reentrancy issue.
    PartitionOptions opts;
    opts.thread_cache = PartitionOptions::kDisabled;
    opts.scheduler_loop_quarantine = PartitionOptions::kDisabled;
    return opts;
  }());

  return *allocator;
}
}  // namespace partition_alloc::internal
