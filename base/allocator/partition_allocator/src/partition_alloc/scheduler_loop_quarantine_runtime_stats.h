// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_RUNTIME_STATS_H_
#define PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_RUNTIME_STATS_H_

#include <vector>

#include "partition_alloc/bucket_lookup.h"
#include "partition_alloc/partition_alloc_base/time/time.h"

namespace partition_alloc::internal {
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) SchedulerLoopQuarantineRuntimeStats {
 public:
  // Public and exported for testing.
  static const size_t kMaxTimesToTrack = 1024;
  struct BucketStats {
    // Should be called to reset everything to its initial state.
    void Reset();
    // Called whenever the stats are exported (resets `paused` and `cycled` but
    // leaves values and current average available).
    void Reported();
    void RecordValue(int64_t value);
    void IncreasePaused() { ++paused_; }

    // const accessors
    bool valid() const { return valid_; }
    int cycled() const { return cycled_; }
    int paused() const { return paused_; }
    int64_t average_ns() const { return average_ns_; }
    int64_t sum_ns() const { return sum_ns_; }
    const std::array<int64_t, kMaxTimesToTrack>& bucket_times() const {
      return bucket_times_;
    }

   private:
    int paused_ = 0;
    int cycled_ = 0;
    bool valid_ = false;
    // updated on each free
    size_t idx_ = 0;
    // Every time stats are reported this value is set to the rolling index
    // of the last value recorded, used to track `cycled` and `valid`, hence we
    // initialize it to `kMaxTimesToTrack - 1` so we only consider it valid
    // after recording `kMaxTimesToTrack` values.
    size_t reported_idx_ = kMaxTimesToTrack - 1;
    int64_t sum_ns_ = 0;
    int64_t average_ns_ = 0;
    std::array<int64_t, kMaxTimesToTrack> bucket_times_ = {};
  };

  // Helper class to track timing of the quarantine method for
  // SchedulerLoopQuarantine.
  template <bool enabled>
  class ThreadScopedStatTracker {
   public:
    PA_ALWAYS_INLINE ThreadScopedStatTracker(
        base::TimeTicks start,
        SchedulerLoopQuarantineRuntimeStats& stats,
        const size_t idx)
        : stats_(stats), bucket_index_(idx) {
      if constexpr (!enabled) {
        return;
      }
      quarantine_start_ = start;
    }
    PA_ALWAYS_INLINE ~ThreadScopedStatTracker() {
      if constexpr (!enabled) {
        return;
      }
      stats_.AddStats(bucket_index_, quarantine_start_, purge_start_,
                      zap_start_, MaybeGetNow(stats_));
    }

    PA_ALWAYS_INLINE void ReportPurgeStart() {
      purge_start_ = MaybeGetNow(stats_);
    }

    PA_ALWAYS_INLINE void ReportZapStart() { zap_start_ = MaybeGetNow(stats_); }

    PA_ALWAYS_INLINE static base::TimeTicks MaybeGetNow(
        const SchedulerLoopQuarantineRuntimeStats& stats) {
      if constexpr (!enabled) {
        return base::TimeTicks();
      }
      if (!stats.IsInitialized()) {
        return base::TimeTicks();
      }
      return base::TimeTicks::Now();
    }

   private:
    SchedulerLoopQuarantineRuntimeStats& stats_;
    const size_t bucket_index_;
    base::TimeTicks quarantine_start_;
    base::TimeTicks purge_start_;
    base::TimeTicks zap_start_;
  };

  SchedulerLoopQuarantineRuntimeStats();
  ~SchedulerLoopQuarantineRuntimeStats();
  SchedulerLoopQuarantineRuntimeStats(
      const SchedulerLoopQuarantineRuntimeStats&) = delete;
  SchedulerLoopQuarantineRuntimeStats(SchedulerLoopQuarantineRuntimeStats&&) =
      delete;

  bool IsInitialized() const { return initialized_; }
  void AddStats(size_t bucket_index,
                const base::TimeTicks quarantine_start,
                const base::TimeTicks purge_start,
                const base::TimeTicks zap_start,
                const base::TimeTicks quarantine_end);
  void InitOrResetStats(base::TimeDelta pause_delay,
                        base::TimeDelta max_above_avg_zap_delta);
  bool ShouldPause(base::TimeTicks start);
  void ReportedStats();

  // const accessors
  const std::vector<BucketStats>& zap_buckets() const;
  const std::vector<BucketStats>& purge_buckets() const;
  const std::vector<BucketStats>& total_time_buckets() const;

 private:
  bool initialized_ = false;
  base::TimeDelta max_above_avg_zap_delta_{};
  base::TimeDelta long_zap_pause_delta_{};
  base::TimeTicks pause_until_;
  std::vector<BucketStats> zap_buckets_;
  std::vector<BucketStats> purge_buckets_;
  std::vector<BucketStats> total_time_buckets_;
};

}  // namespace partition_alloc::internal
#endif  // PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_RUNTIME_STATS_H_
