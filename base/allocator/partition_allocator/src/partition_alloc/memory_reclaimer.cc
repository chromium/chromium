// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/memory_reclaimer.h"

#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"

#if PA_BUILDFLAG(USE_STARSCAN)
#include "partition_alloc/starscan/pcscan.h"
#endif

namespace partition_alloc {

// static
MemoryReclaimer* MemoryReclaimer::Instance() {
  static internal::base::NoDestructor<MemoryReclaimer> instance;
  return instance.get();
}

void MemoryReclaimer::RegisterPartition(PartitionRoot* partition) {
  internal::ScopedGuard lock(lock_);
  PA_DCHECK(partition);
  auto it_and_whether_inserted = partitions_.insert(partition);
  PA_DCHECK(it_and_whether_inserted.second);
}

void MemoryReclaimer::UnregisterPartition(PartitionRoot* partition) {
  internal::ScopedGuard lock(lock_);
  PA_DCHECK(partition);
  size_t erased_count = partitions_.erase(partition);
  PA_DCHECK(erased_count == 1u);
}

MemoryReclaimer::MemoryReclaimer() = default;
MemoryReclaimer::~MemoryReclaimer() = default;

void MemoryReclaimer::ReclaimAll() {
  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages |
                         PurgeFlags::kAggressiveReclaim;
  Reclaim(kFlags);
}

void MemoryReclaimer::ReclaimNormal() {
  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages;
  Reclaim(kFlags);
}

void MemoryReclaimer::ReclaimFast() {
  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages |
                         PurgeFlags::kLimitDuration;
  Reclaim(kFlags);
}

void MemoryReclaimer::Reclaim(int flags) {
  internal::ScopedGuard lock(
      lock_);  // Has to protect from concurrent (Un)Register calls.

  // PCScan quarantines freed slots. Trigger the scan first to let it call
  // FreeNoHooksImmediate on slots that pass the quarantine.
  //
  // In turn, FreeNoHooksImmediate may add slots to thread cache. Purge it next
  // so that the slots are actually freed. (This is done synchronously only for
  // the current thread.)
  //
  // Lastly decommit empty slot spans and lastly try to discard unused pages at
  // the end of the remaining active slots.
#if PA_CONFIG(STARSCAN_ENABLE_STARSCAN_ON_RECLAIM) && PA_BUILDFLAG(USE_STARSCAN)
  {
    using PCScan = internal::PCScan;
    const auto invocation_mode = flags & PurgeFlags::kAggressiveReclaim
                                     ? PCScan::InvocationMode::kForcedBlocking
                                     : PCScan::InvocationMode::kBlocking;
    PCScan::PerformScanIfNeeded(invocation_mode);
  }
#endif  // PA_CONFIG(STARSCAN_ENABLE_STARSCAN_ON_RECLAIM) &&
        // PA_BUILDFLAG(USE_STARSCAN)

#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
  // Don't completely empty the thread cache outside of low memory situations,
  // as there is periodic purge which makes sure that it doesn't take too much
  // space.
  if (flags & PurgeFlags::kAggressiveReclaim) {
    ThreadCacheRegistry::Instance().PurgeAll();
  }
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)

  for (auto* partition : partitions_) {
    partition->PurgeMemory(flags);
  }
}

void MemoryReclaimer::ResetForTesting() {
  internal::ScopedGuard lock(lock_);
  partitions_.clear();
}

}  // namespace partition_alloc
