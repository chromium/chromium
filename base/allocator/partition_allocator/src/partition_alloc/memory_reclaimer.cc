// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/memory_reclaimer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/partition_root_internal.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"

namespace partition_alloc {

// static
MemoryReclaimer* MemoryReclaimer::Instance() {
  static internal::base::NoDestructor<MemoryReclaimer> instance;
  return instance.get();
}

void MemoryReclaimer::RegisterPartition(PartitionRoot* partition) {
  internal::ScopedGuard lock(lock_);
  PA_DCHECK(partition);
  auto it_and_whether_inserted = partitions_.try_emplace(partition);
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

void MemoryReclaimer::ReclaimFast() {
  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages |
                         PurgeFlags::kLimitDuration;
  Reclaim(kFlags);
}

void MemoryReclaimer::ReclaimForTesting() {
  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages;
  Reclaim(kFlags);
}

void MemoryReclaimer::Reclaim(int flags) {
  internal::ScopedGuard lock(
      lock_);  // Has to protect from concurrent (Un)Register calls.

#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
  // Don't completely empty the thread cache outside of low memory situations,
  // as there is periodic purge which makes sure that it doesn't take too much
  // space.
  if (flags & PurgeFlags::kAggressiveReclaim) {
    ThreadCache::PurgeAllThread();
  }
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)

  size_t total_decommitted_bytes = 0;
  for (auto& partition : partitions_) {
    total_decommitted_bytes +=
        partition.first->PurgeMemory(flags, partition.second)
            .decommitted_empty_slot_spans_bytes;
  }

  if (adaptive_interval_config_.enabled) {
    // The signal driving the back-off is the memory that was held in empty
    // slot spans, which is exactly what PurgeFlags::kDecommitEmptySlotSpans
    // just reclaimed. Taken from the purge itself so that this costs no extra
    // per-partition locking, as opposed to a DumpStats() walk of every slot
    // span.
    const internal::base::TimeDelta next_interval = ComputeNextReclaimInterval(
        adaptive_interval_config_,
        internal::base::Microseconds(
            next_reclaim_interval_micros_.load(std::memory_order_relaxed)),
        total_decommitted_bytes);
    next_reclaim_interval_micros_.store(next_interval.InMicroseconds(),
                                        std::memory_order_relaxed);
  }
}

void MemoryReclaimer::ResetForTesting() {
  // Called from a test, when it should be a single threaded context.
  internal::ScopedGuard lock(lock_);
  partitions_.clear();
  adaptive_interval_config_ = AdaptiveIntervalConfig();
  next_reclaim_interval_micros_.store(0, std::memory_order_relaxed);
}

// static
internal::base::TimeDelta MemoryReclaimer::ComputeNextReclaimInterval(
    const AdaptiveIntervalConfig& config,
    internal::base::TimeDelta current_interval,
    size_t total_decommitted_bytes) {
  // `min_decommittable_bytes` comes from a field trial, so cap it low enough
  // that the multiplications below cannot wrap size_t on 32-bit builds. A
  // watermark that large is unreachable anyway, so the policy just keeps
  // backing off, which is the safe direction.
  const size_t min_bytes = std::min(config.min_decommittable_bytes,
                                    std::numeric_limits<size_t>::max() / 10);
  if (total_decommitted_bytes > 10 * min_bytes) {
    // A lot to reclaim: on top of halving, cap the interval at
    // `default_interval`, so that heavy reclaim always runs at least as often
    // as the default cadence.
    current_interval = std::min(config.default_interval, current_interval / 2);
  } else if (total_decommitted_bytes > 2 * min_bytes) {
    current_interval = std::max(config.min_interval, current_interval / 2);
  } else if (total_decommitted_bytes < min_bytes) {
    current_interval = std::min(config.max_interval, current_interval * 2);
  }
  // In [min_bytes, 2 * min_bytes] the interval is left alone, which gives the
  // policy some hysteresis.
  return std::clamp(current_interval, config.min_interval, config.max_interval);
}

void MemoryReclaimer::SetAdaptiveIntervalConfig(
    const AdaptiveIntervalConfig& config) {
  // The values may come from a field trial. A configuration the state machine
  // cannot run on is a bug in whatever produced it, so DCHECK each condition
  // next to the repair that keeps release builds going, making it explicit
  // that production never relies on those DCHECKs.
  AdaptiveIntervalConfig sanitized = config;
  const AdaptiveIntervalConfig defaults;

  // A non-positive interval would make the embedder reschedule with no delay.
  PA_DCHECK(config.min_interval.is_positive());
  if (!sanitized.min_interval.is_positive()) {
    sanitized.min_interval = defaults.min_interval;
  }

  PA_DCHECK(config.max_interval >= config.min_interval);
  sanitized.max_interval =
      std::max(sanitized.max_interval, sanitized.min_interval);

  PA_DCHECK(config.default_interval >= config.min_interval);
  PA_DCHECK(config.default_interval <= config.max_interval);
  sanitized.default_interval =
      std::clamp(sanitized.default_interval, sanitized.min_interval,
                 sanitized.max_interval);

  PA_DCHECK(config.min_decommittable_bytes > 0);
  if (sanitized.min_decommittable_bytes == 0) {
    sanitized.min_decommittable_bytes = 1;
  }

  {
    internal::ScopedGuard lock(lock_);
    adaptive_interval_config_ = sanitized;
  }
  next_reclaim_interval_micros_.store(
      sanitized.enabled ? sanitized.default_interval.InMicroseconds() : 0,
      std::memory_order_relaxed);
}

MemoryReclaimer::AdaptiveIntervalConfig
MemoryReclaimer::GetSanitizedAdaptiveIntervalConfigForTesting() {
  internal::ScopedGuard lock(lock_);
  return adaptive_interval_config_;
}

int64_t MemoryReclaimer::GetRecommendedReclaimIntervalInMicroseconds() const {
  // Written by Reclaim() under `lock_` and by SetAdaptiveIntervalConfig(), but
  // read here without the lock, from the embedder's scheduling sequence.
  // Relaxed ordering is enough because this value publishes no other state: it
  // only picks the delay of the next reclaim, so observing a stale interval
  // merely means one more wake-up at the previous cadence.
  const int64_t adaptive_interval_micros =
      next_reclaim_interval_micros_.load(std::memory_order_relaxed);
  if (adaptive_interval_micros > 0) {
    return adaptive_interval_micros;
  }
  return internal::base::Seconds(4).InMicroseconds();
}

}  // namespace partition_alloc
