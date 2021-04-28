// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/memory_reclaimer.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/base_tracing.h"

namespace base {

namespace {

template <bool thread_safe>
void Insert(std::set<PartitionRoot<thread_safe>*>* partitions,
            PartitionRoot<thread_safe>* partition) {
  PA_DCHECK(partition);
  auto it_and_whether_inserted = partitions->insert(partition);
  PA_DCHECK(it_and_whether_inserted.second);
}

template <bool thread_safe>
void Remove(std::set<PartitionRoot<thread_safe>*>* partitions,
            PartitionRoot<thread_safe>* partition) {
  PA_DCHECK(partition);
  size_t erased_count = partitions->erase(partition);
  PA_DCHECK(erased_count == 1u);
}

}  // namespace

// static
PartitionAllocMemoryReclaimer* PartitionAllocMemoryReclaimer::Instance() {
  static NoDestructor<PartitionAllocMemoryReclaimer> instance;
  return instance.get();
}

void PartitionAllocMemoryReclaimer::RegisterPartition(
    PartitionRoot<internal::ThreadSafe>* partition) {
  AutoLock lock(lock_);
  Insert(&thread_safe_partitions_, partition);
}

void PartitionAllocMemoryReclaimer::RegisterPartition(
    PartitionRoot<internal::NotThreadSafe>* partition) {
  AutoLock lock(lock_);
  Insert(&thread_unsafe_partitions_, partition);
}

void PartitionAllocMemoryReclaimer::UnregisterPartition(
    PartitionRoot<internal::ThreadSafe>* partition) {
  AutoLock lock(lock_);
  Remove(&thread_safe_partitions_, partition);
}

void PartitionAllocMemoryReclaimer::UnregisterPartition(
    PartitionRoot<internal::NotThreadSafe>* partition) {
  AutoLock lock(lock_);
  Remove(&thread_unsafe_partitions_, partition);
}

void PartitionAllocMemoryReclaimer::Start(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  PA_DCHECK(!timer_);
  PA_DCHECK(task_runner);

  {
    AutoLock lock(lock_);
    PA_DCHECK(!thread_safe_partitions_.empty());
  }

  // This does not need to run on the main thread, however there are a few
  // reasons to do it there:
  // - Most of PartitionAlloc's usage is on the main thread, hence PA's metadata
  //   is more likely in cache when executing on the main thread.
  // - Memory reclaim takes the partition lock for each partition. As a
  //   consequence, while reclaim is running, the main thread is unlikely to be
  //   able to make progress, as it would be waiting on the lock.
  // - Finally, this runs in idle time only, so there should be no visible
  //   impact.
  //
  // From local testing, time to reclaim is 100us-1ms, and reclaiming every few
  // seconds is useful. Since this is meant to run during idle time only, it is
  // a reasonable starting point balancing effectivenes vs cost. See
  // crbug.com/942512 for details and experimental results.
  constexpr TimeDelta kInterval = TimeDelta::FromSeconds(4);

  timer_ = std::make_unique<RepeatingTimer>();
  timer_->SetTaskRunner(task_runner);
  // Here and below, |Unretained(this)| is fine as |this| lives forever, as a
  // singleton.
  timer_->Start(
      FROM_HERE, kInterval,
      BindRepeating(&PartitionAllocMemoryReclaimer::ReclaimPeriodically,
                    Unretained(this)));
}

PartitionAllocMemoryReclaimer::PartitionAllocMemoryReclaimer() = default;
PartitionAllocMemoryReclaimer::~PartitionAllocMemoryReclaimer() = default;

void PartitionAllocMemoryReclaimer::ReclaimAll() {
  constexpr int kFlags = PartitionPurgeDecommitEmptySlotSpans |
                         PartitionPurgeDiscardUnusedSystemPages |
                         PartitionPurgeAggressiveReclaim;
  Reclaim(kFlags);
}

void PartitionAllocMemoryReclaimer::ReclaimPeriodically() {
  constexpr int kFlags = PartitionPurgeDecommitEmptySlotSpans |
                         PartitionPurgeDiscardUnusedSystemPages;
  Reclaim(kFlags);
}

void PartitionAllocMemoryReclaimer::Reclaim(int flags) {
  AutoLock lock(lock_);  // Has to protect from concurrent (Un)Register calls.
  TRACE_EVENT0("base", "PartitionAllocMemoryReclaimer::Reclaim()");

  // PCScan quarantines freed slots. Trigger the scan first to let it call
  // FreeNoHooksImmediate on slots that pass the quarantine.
  //
  // In turn, FreeNoHooksImmediate may add slots to thread cache. Purge it next
  // so that the slots are actually freed. (This is done synchronously only for
  // the current thread.)
  //
  // Lastly decommit empty slot spans and lastly try to discard unused pages at
  // the end of the remaining active slots.
  {
    using PCScan = internal::PCScan;
    const auto invocation_mode = flags & PartitionPurgeAggressiveReclaim
                                     ? PCScan::InvocationMode::kForcedBlocking
                                     : PCScan::InvocationMode::kBlocking;
    PCScan::PerformScanIfNeeded(invocation_mode);
  }

#if defined(PA_THREAD_CACHE_SUPPORTED)
  // Don't completely empty the thread cache outside of low memory situations,
  // as there is periodic purge which makes sure that it doesn't take too much
  // space.
  if (flags & PartitionPurgeAggressiveReclaim)
    internal::ThreadCacheRegistry::Instance().PurgeAll();
#endif

  for (auto* partition : thread_safe_partitions_)
    partition->PurgeMemory(flags);
  for (auto* partition : thread_unsafe_partitions_)
    partition->PurgeMemory(flags);
}

void PartitionAllocMemoryReclaimer::ResetForTesting() {
  AutoLock lock(lock_);

  timer_ = nullptr;
  thread_safe_partitions_.clear();
  thread_unsafe_partitions_.clear();
}

}  // namespace base
