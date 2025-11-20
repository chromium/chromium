// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_MEMORY_RECLAIMER_H_
#define PARTITION_ALLOC_MEMORY_RECLAIMER_H_

#include <memory>
#include <set>

#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc {

// Posts and handles memory reclaim tasks for PartitionAlloc.
//
// PartitionAlloc users are responsible for scheduling and calling the
// reclamation methods with their own timers / event loops.
//
// Singleton as this runs as long as the process is alive, and
// having multiple instances would be wasteful.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) MemoryReclaimer {
 public:
  static MemoryReclaimer* Instance();

  MemoryReclaimer(const MemoryReclaimer&) = delete;
  MemoryReclaimer& operator=(const MemoryReclaimer&) = delete;

  // Internal. Do not use.
  // Registers a partition to be tracked by the reclaimer.
  void RegisterPartition(PartitionRoot* partition) PA_LOCKS_EXCLUDED(lock_);
  // Internal. Do not use.
  // Unregisters a partition to be tracked by the reclaimer.
  void UnregisterPartition(PartitionRoot* partition) PA_LOCKS_EXCLUDED(lock_);

  // Returns a recommended interval to invoke ReclaimNormal.
  int64_t GetRecommendedReclaimIntervalInMicroseconds() {
    return internal::base::Seconds(4).InMicroseconds();
  }

  // Triggers an explicit reclaim now reclaiming all free memory
  void ReclaimAll() PA_LOCKS_EXCLUDED(lock_);
  // Same as ReclaimNormal(), but return early if reclaim takes too long.
  void ReclaimFast() PA_LOCKS_EXCLUDED(lock_);
  // Same as above, but does not limit reclaim time to avoid test flakiness.
  void ReclaimForTesting() PA_LOCKS_EXCLUDED(lock_);

 private:
  MemoryReclaimer();
  ~MemoryReclaimer();
  // |flags| is an OR of base::PartitionPurgeFlags
  void Reclaim(int flags) PA_LOCKS_EXCLUDED(lock_);
  void ResetForTesting() PA_LOCKS_EXCLUDED(lock_);

  internal::Lock lock_;
  std::set<PartitionRoot*> partitions_ PA_GUARDED_BY(lock_);

  friend class internal::base::NoDestructor<MemoryReclaimer>;
  friend class MemoryReclaimerTest;
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_MEMORY_RECLAIMER_H_
