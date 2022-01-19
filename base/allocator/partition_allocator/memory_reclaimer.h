// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_

#include <memory>
#include <set>

#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/no_destructor.h"
#include "base/thread_annotations.h"

namespace base {

// Posts and handles memory reclaim tasks for PartitionAlloc.
//
// Thread safety: |RegisterPartition()| and |UnregisterPartition()| can be
// called from any thread, concurrently with reclaim. Reclaim itself runs in the
// context of the provided |SequencedTaskRunner|, meaning that the caller must
// take care of this runner being compatible with the various partitions.
//
// Singleton as this runs as long as the process is alive, and
// having multiple instances would be wasteful.
class BASE_EXPORT PartitionAllocMemoryReclaimer {
 public:
  static PartitionAllocMemoryReclaimer* Instance();

  PartitionAllocMemoryReclaimer(const PartitionAllocMemoryReclaimer&) = delete;
  PartitionAllocMemoryReclaimer& operator=(
      const PartitionAllocMemoryReclaimer&) = delete;

  // Internal. Do not use.
  // Registers a partition to be tracked by the reclaimer.
  void RegisterPartition(PartitionRoot<>* partition);
  // Internal. Do not use.
  // Unregisters a partition to be tracked by the reclaimer.
  void UnregisterPartition(PartitionRoot<>* partition);

  // Triggers an explicit reclaim now to reclaim as much free memory as
  // possible. The API callers need to invoke this method periodically
  // if they want to use memory reclaimer.
  // See also GetRecommendedReclaimIntervalInMicroseconds()'s comment.
  void ReclaimNormal();

  // Returns a recommended interval to invoke ReclaimNormal.
  int64_t GetRecommendedReclaimIntervalInMicroseconds() {
    return Seconds(4).InMicroseconds();
  }

  // Triggers an explicit reclaim now reclaiming all free memory
  void ReclaimAll();

 private:
  PartitionAllocMemoryReclaimer();
  ~PartitionAllocMemoryReclaimer();
  // |flags| is an OR of base::PartitionPurgeFlags
  void Reclaim(int flags);
  void ReclaimAndReschedule();
  void ResetForTesting();

  internal::PartitionLock lock_;
  std::set<PartitionRoot<>*> partitions_ GUARDED_BY(lock_);

  friend class NoDestructor<PartitionAllocMemoryReclaimer>;
  friend class PartitionAllocMemoryReclaimerTest;
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_
