// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "partition_alloc/scheduler_loop_quarantine_runtime_stats.h"

namespace partition_alloc::internal {

// class RuntimeStats methods.
SchedulerLoopQuarantineRuntimeStats::SchedulerLoopQuarantineRuntimeStats() =
    default;

SchedulerLoopQuarantineRuntimeStats::~SchedulerLoopQuarantineRuntimeStats() =
    default;

void SchedulerLoopQuarantineRuntimeStats::AddStats(
    size_t bucket_index,
    const base::TimeTicks quarantine_start,
    const base::TimeTicks purge_start,
    const base::TimeTicks zap_start,
    const base::TimeTicks quarantine_end) {
  if (!initialized_) {
    return;
  }
  // These should always be set.
  PA_DCHECK(!quarantine_start.is_null());
  PA_DCHECK(!quarantine_end.is_null());

  total_time_buckets_[bucket_index].RecordValue(
      (quarantine_end - quarantine_start).InNanoseconds());

  // Zap is last but has a separate feature to enable it, so might be null.
  base::TimeDelta zap_time;
  int64_t average_ns = zap_buckets_[bucket_index].average_ns();
  if (!zap_start.is_null()) {
    // If we zapped then the quarantine is active and we should have a purge
    // time.
    PA_DCHECK(!purge_start.is_null());
    zap_time = quarantine_end - zap_start;
    purge_buckets_[bucket_index].RecordValue(
        (zap_start - purge_start).InNanoseconds());
    zap_buckets_[bucket_index].RecordValue(zap_time.InNanoseconds());
  } else if (!purge_start.is_null()) {
    // If we didn't zap we measure purge time to the end.
    purge_buckets_[bucket_index].RecordValue(
        (quarantine_end - purge_start).InNanoseconds());
  }

  // If we have a valid zap we should decide if this should cause us to pause
  // the quarantine (zap time was above the average by to much).
  const bool should_pause_on_long_zap =
      !max_above_avg_zap_delta_.is_zero() && !zap_time.is_zero();
  if (!zap_buckets_[bucket_index].valid() || !should_pause_on_long_zap) {
    return;
  }
  if (zap_time - base::Nanoseconds(average_ns) > max_above_avg_zap_delta_) {
    // This should be enforced, if we have `max_above_avg_zap_delta_` we should
    // have a `long_zap_pause_delta_`.
    PA_DCHECK(!long_zap_pause_delta_.is_zero());
    pause_until_ = quarantine_end + long_zap_pause_delta_;
    zap_buckets_[bucket_index].IncreasePaused();
  }
}

void SchedulerLoopQuarantineRuntimeStats::InitOrResetStats(
    base::TimeDelta pause_delay,
    base::TimeDelta max_above_avg_zap_delta) {
  if (!initialized_) {
    // Only do this once, if we never record anything we don't need to
    // allocate.
    initialized_ = true;
    zap_buckets_.resize(BucketIndexLookup::kNumBuckets);
    purge_buckets_.resize(BucketIndexLookup::kNumBuckets);
    total_time_buckets_.resize(BucketIndexLookup::kNumBuckets);
  } else {
    for (auto& stat : zap_buckets_) {
      stat.Reset();
    }
    for (auto& stat : purge_buckets_) {
      stat.Reset();
    }
    for (auto& stat : total_time_buckets_) {
      stat.Reset();
    }
  }
  long_zap_pause_delta_ = pause_delay;
  max_above_avg_zap_delta_ = max_above_avg_zap_delta;
}

bool SchedulerLoopQuarantineRuntimeStats::ShouldPause(base::TimeTicks start) {
  if (!initialized_ || pause_until_.is_null() || start.is_null()) {
    return false;
  }
  return start < pause_until_;
}
void SchedulerLoopQuarantineRuntimeStats::ReportedStats() {
  if (!initialized_) {
    return;
  }
  for (auto& stat : zap_buckets_) {
    stat.Reported();
  }
  for (auto& stat : purge_buckets_) {
    stat.Reported();
  }
  for (auto& stat : total_time_buckets_) {
    stat.Reported();
  }
}

const std::vector<SchedulerLoopQuarantineRuntimeStats::BucketStats>&
SchedulerLoopQuarantineRuntimeStats::zap_buckets() const {
  return zap_buckets_;
}
const std::vector<SchedulerLoopQuarantineRuntimeStats::BucketStats>&
SchedulerLoopQuarantineRuntimeStats::purge_buckets() const {
  return purge_buckets_;
}
const std::vector<SchedulerLoopQuarantineRuntimeStats::BucketStats>&
SchedulerLoopQuarantineRuntimeStats::total_time_buckets() const {
  return total_time_buckets_;
}

// class RuntimeStats::BucketStats method.
void SchedulerLoopQuarantineRuntimeStats::BucketStats::Reset() {
  valid_ = false;
  idx_ = 0;
  sum_ns_ = 0;
  average_ns_ = 0;
  reported_idx_ = kMaxTimesToTrack - 1;
}
void SchedulerLoopQuarantineRuntimeStats::BucketStats::Reported() {
  if (valid_) {
    paused_ = 0;
    cycled_ = 0;
    reported_idx_ = idx_;
  }
}
void SchedulerLoopQuarantineRuntimeStats::BucketStats::RecordValue(
    int64_t value_ns) {
  sum_ns_ =
      sum_ns_ + (value_ns ? value_ns : 1) - (valid_ ? bucket_times_[idx_] : 0);
  bucket_times_[idx_] = value_ns;
  if (idx_ == reported_idx_) {
    valid_ = true;
    ++cycled_;
  }
  if (idx_ == bucket_times_.size() - 1) {
    idx_ = 0;
  } else {
    ++idx_;
  }
  if (valid_) {
    // integer division means some loss of precision in the average, but
    // avoids the cost of floating point division. Also the array is a
    // power of 2 and thus should optimize nicely.
    average_ns_ = sum_ns_ / kMaxTimesToTrack;
  }
}
}  // namespace partition_alloc::internal
