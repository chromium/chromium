// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_MEMORY_RECLAIMER_H_
#define PARTITION_ALLOC_MEMORY_RECLAIMER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

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
  //
  // While the adaptive interval is enabled (see AdaptiveIntervalConfig below)
  // this can change the interval each reclaim based on how much memory was
  // reclaimed, otherwise it is a fixed interval.
  int64_t GetRecommendedReclaimIntervalInMicroseconds() const;

  // Runtime-tunable configuration of the adaptive periodic reclaim interval.
  //
  // When enabled, the interval adapts to how much there is to reclaim: the
  // reclaimer wakes up less often (saving power) while there is little
  // decommittable memory, and more often once there is a lot. This mirrors the
  // adaptive back-off the thread cache periodic purge already uses (see
  // ThreadCacheRegistry::RunPeriodicPurge()).
  //
  // The signal is how much each reclaim actually decommitted, reported by
  // PartitionRoot::PurgeMemory().
  //
  // Starting from `default_interval`, the interval doubles toward
  // `max_interval` while the decommitted memory stays below
  // `min_decommittable_bytes`, and halves toward `min_interval` once it
  // exceeds twice that watermark. Beyond ten times the watermark it is also
  // capped at `default_interval`, so a burst of reclaimable memory always
  // brings the cadence back to at least the default one. Every result is
  // clamped to [`min_interval`, `max_interval`]. See
  // ComputeNextReclaimInterval(), which is authoritative.
  //
  // Defaults are the values used when the feature is off, so a
  // default-constructed config is inert.
  struct AdaptiveIntervalConfig {
    // Whether the interval adapts at all. While false, Reclaim() does no extra
    // work and GetRecommendedReclaimIntervalInMicroseconds() keeps returning
    // the fixed interval.
    bool enabled = false;
    internal::base::TimeDelta min_interval = internal::base::Seconds(4);
    internal::base::TimeDelta max_interval = internal::base::Minutes(1);
    internal::base::TimeDelta default_interval = internal::base::Seconds(8);
    size_t min_decommittable_bytes = 100 * 1024;
  };

  // Overrides the adaptive interval configuration and resets the current
  // interval to `config.default_interval`. Meant to be called once during
  // startup, before the reclaimer is scheduled.
  //
  // `config` must describe a usable state machine: positive intervals,
  // `min_interval` <= `default_interval` <= `max_interval` and a non-zero
  // watermark. Anything else is a configuration bug and DCHECKs.
  void SetAdaptiveIntervalConfig(const AdaptiveIntervalConfig& config)
      PA_LOCKS_EXCLUDED(lock_);

  // Returns the configuration currently in effect, i.e. `config` as last
  // passed to SetAdaptiveIntervalConfig(), after any release-build repairs.
  AdaptiveIntervalConfig GetSanitizedAdaptiveIntervalConfigForTesting()
      PA_LOCKS_EXCLUDED(lock_);

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

  // Computes the next periodic reclaim interval from `config`, the current
  // interval and the number of bytes the reclaim that just ran decommitted
  // across all registered partitions. A pure function so that the back-off
  // policy can be unit-tested in isolation, without having to fabricate real
  // decommittable memory.
  static internal::base::TimeDelta ComputeNextReclaimInterval(
      const AdaptiveIntervalConfig& config,
      internal::base::TimeDelta current_interval,
      size_t total_decommitted_bytes);

  internal::Lock lock_;
  std::map<PartitionRoot*, PurgeState> partitions_ PA_GUARDED_BY(lock_);

  // Written by SetAdaptiveIntervalConfig() and read by Reclaim(); both hold
  // `lock_`.
  AdaptiveIntervalConfig adaptive_interval_config_ PA_GUARDED_BY(lock_);

  // Next periodic reclaim interval in microseconds, or 0 while the adaptive
  // interval is disabled. Accessed without `lock_`: read from
  // GetRecommendedReclaimIntervalInMicroseconds(), which runs on the embedder's
  // scheduling sequence and so may race with a reclaim, and written by
  // SetAdaptiveIntervalConfig(), which startup calls before scheduling.
  // Atomic so that those lock-free accesses are well defined.
  std::atomic<int64_t> next_reclaim_interval_micros_{0};

  friend class internal::base::NoDestructor<MemoryReclaimer>;
  friend class MemoryReclaimerTest;
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_MEMORY_RECLAIMER_H_
