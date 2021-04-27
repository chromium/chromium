// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_

#include <memory>
#include <set>

#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"

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

  // Internal. Do not use.
  // Registers a partition to be tracked by the reclaimer.
  void RegisterPartition(PartitionRoot<internal::ThreadSafe>* partition);
  void RegisterPartition(PartitionRoot<internal::NotThreadSafe>* partition);
  // Internal. Do not use.
  // Unregisters a partition to be tracked by the reclaimer.
  void UnregisterPartition(PartitionRoot<internal::ThreadSafe>* partition);
  void UnregisterPartition(PartitionRoot<internal::NotThreadSafe>* partition);
  // Starts the periodic reclaim. Should be called once.
  void Start(scoped_refptr<SequencedTaskRunner> task_runner);
  // Triggers an explicit reclaim now reclaiming all free memory
  void ReclaimAll();
  // Triggers an explicit reclaim now to reclaim as much free memory as
  // possible.
  void ReclaimPeriodically();

 private:
  PartitionAllocMemoryReclaimer();
  ~PartitionAllocMemoryReclaimer();
  // |flags| is an OR of base::PartitionPurgeFlags
  void Reclaim(int flags);
  void ReclaimAndReschedule();
  void ResetForTesting();

  // Schedules periodic |Reclaim()|.
  std::unique_ptr<RepeatingTimer> timer_;

  Lock lock_;
  std::set<PartitionRoot<internal::ThreadSafe>*> thread_safe_partitions_
      GUARDED_BY(lock_);
  std::set<PartitionRoot<internal::NotThreadSafe>*> thread_unsafe_partitions_
      GUARDED_BY(lock_);

  friend class NoDestructor<PartitionAllocMemoryReclaimer>;
  friend class PartitionAllocMemoryReclaimerTest;
  DISALLOW_COPY_AND_ASSIGN(PartitionAllocMemoryReclaimer);
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_
