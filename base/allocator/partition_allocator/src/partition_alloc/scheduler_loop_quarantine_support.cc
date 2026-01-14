// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/scheduler_loop_quarantine_support.h"

#include "partition_alloc/partition_root.h"

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

SchedulerLoopQuarantineScanPolicyUpdater::
    SchedulerLoopQuarantineScanPolicyUpdater() = default;

SchedulerLoopQuarantineScanPolicyUpdater::
    ~SchedulerLoopQuarantineScanPolicyUpdater() {
  // Ensure all `DisallowScanlessPurge()` calls were followed by
  // `AllowScanlessPurge()`.
  PA_CHECK(disallow_scanless_purge_calls_ == 0);
}

void SchedulerLoopQuarantineScanPolicyUpdater::DisallowScanlessPurge() {
  disallow_scanless_purge_calls_++;
  PA_CHECK(0 < disallow_scanless_purge_calls_);  // Overflow check.

  auto* branch = GetQuarantineBranch();
  PA_CHECK(branch);
  branch->DisallowScanlessPurge();
}

void SchedulerLoopQuarantineScanPolicyUpdater::AllowScanlessPurge() {
  PA_CHECK(0 < disallow_scanless_purge_calls_);
  disallow_scanless_purge_calls_--;

  auto* branch = GetQuarantineBranch();
  PA_CHECK(branch);
  branch->AllowScanlessPurge();
}

internal::ThreadBoundSchedulerLoopQuarantineBranch*
SchedulerLoopQuarantineScanPolicyUpdater::GetQuarantineBranch() {
  ThreadCache* tcache = ThreadCache::EnsureAndGet();
  if (!ThreadCache::IsValid(tcache)) {
    return nullptr;
  }

  uintptr_t current_tcache_addr = reinterpret_cast<uintptr_t>(tcache);
  if (tcache_address_ == 0) {
    tcache_address_ = current_tcache_addr;
  } else {
    PA_CHECK(tcache_address_ == current_tcache_addr);
  }
  return &tcache->GetSchedulerLoopQuarantineBranch();
}

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
