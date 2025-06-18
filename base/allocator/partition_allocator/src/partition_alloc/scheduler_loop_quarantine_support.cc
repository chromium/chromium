// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/scheduler_loop_quarantine_support.h"

namespace partition_alloc {
ScopedSchedulerLoopQuarantineExclusion::
    ScopedSchedulerLoopQuarantineExclusion() {
  ThreadCache* tcache = ThreadCache::Get();
  if (!ThreadCache::IsValid(tcache)) {
    return;
  }
  instance_.emplace(tcache->GetSchedulerLoopQuarantineBranch());
}
ScopedSchedulerLoopQuarantineExclusion::
    ~ScopedSchedulerLoopQuarantineExclusion() {}

namespace internal {
ScopedSchedulerLoopQuarantineBranchAccessorForTesting::
    ScopedSchedulerLoopQuarantineBranchAccessorForTesting(
        PartitionRoot* allocator_root) {
  if (allocator_root->settings.with_thread_cache) {
    ThreadCache* tcache = ThreadCache::Get();
    if (ThreadCache::IsValid(tcache)) {
      branch_ = &tcache->GetSchedulerLoopQuarantineBranch();
      return;
    }
  }
  branch_ = &allocator_root->scheduler_loop_quarantine;
}

ScopedSchedulerLoopQuarantineBranchAccessorForTesting::
    ~ScopedSchedulerLoopQuarantineBranchAccessorForTesting() = default;

bool ScopedSchedulerLoopQuarantineBranchAccessorForTesting::IsQuarantined(
    void* object) {
  if (branch_.index() == 0) {
    return std::get<0>(branch_)->IsQuarantinedForTesting(object);
  } else {
    return std::get<1>(branch_)->IsQuarantinedForTesting(object);
  }
}

size_t
ScopedSchedulerLoopQuarantineBranchAccessorForTesting::GetCapacityInBytes() {
  if (branch_.index() == 0) {
    return std::get<0>(branch_)->GetCapacityInBytes();
  } else {
    return std::get<1>(branch_)->GetCapacityInBytes();
  }
}

void ScopedSchedulerLoopQuarantineBranchAccessorForTesting::Purge() {
  if (branch_.index() == 0) {
    std::get<0>(branch_)->Purge();
  } else {
    std::get<1>(branch_)->Purge();
  }
}
}  // namespace internal
}  // namespace partition_alloc
