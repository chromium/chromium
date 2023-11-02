// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/recent_events_counter.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace ash {
namespace power {
namespace ml {

RecentEventsCounter::RecentEventsCounter(base::TimeDelta duration,
                                         int num_buckets)
    : duration_(duration), num_buckets_(num_buckets) {
  DCHECK_GT(num_buckets_, 0);
  bucket_duration_ = duration_ / num_buckets_;
  DCHECK_EQ(duration_, bucket_duration_ * num_buckets_);
  event_count_.resize(num_buckets_, 0);
}

RecentEventsCounter::~RecentEventsCounter() = default;

void RecentEventsCounter::Log(base::TimeDelta timestamp) {
  if (timestamp < first_bucket_time_) {
    // This event is too old to log.
    return;
  }
  if (timestamp > latest_) {
    latest_ = timestamp;
  }
  const int bucket_index = GetBucketIndex(timestamp);
  if (timestamp < first_bucket_time_ + duration_) {
    // The event is within the current time window so increment the bucket.
    event_count_[bucket_index]++;
    return;
  }

  if (timestamp >= first_bucket_time_ + 2 * duration_) {
    // The event is later than the current window for the existing data, by MORE
    // than `duration` -> zero all the buckets.
    std::fill(event_count_.begin(), event_count_.end(), 0);
  } else {
    // The event is later than the current window for the existing data, by LESS
    // than `duration` -> zero the buckets between the old `first_bucket_index_`
    // and this event's `bucket_index`.
    for (int i = first_bucket_index_; i != bucket_index;
         i = (i + 1) % num_buckets_) {
      event_count_[i] = 0;
    }
  }

  event_count_[bucket_index] = 1;
  first_bucket_index_ = (bucket_index + 1) % num_buckets_;

  // Move the first bucket time such that |bucket_index| is the last bucket in
  // the period [first_bucket_time_, first_bucket_time_ + duration_).
  first_bucket_time_ =
      timestamp - (timestamp % bucket_duration_) + bucket_duration_ - duration_;
}

int RecentEventsCounter::GetTotal(base::TimeDelta now) const {
  DCHECK_GE(now, latest_);
  if (now >= first_bucket_time_ + 2 * duration_) {
    return 0;
  }
  int total = 0;
  const base::TimeDelta start =
      std::max(first_bucket_time_, now - duration_ + bucket_duration_);
  const base::TimeDelta end =
      std::min(now, first_bucket_time_ + duration_ - bucket_duration_);
  const int end_index = GetBucketIndex(end);
  for (int i = GetBucketIndex(start); i != end_index;
       i = (i + 1) % num_buckets_) {
    total += event_count_[i];
  }
  total += event_count_[end_index];
  return total;
}

int RecentEventsCounter::GetBucketIndex(base::TimeDelta timestamp) const {
  DCHECK_GE(timestamp, base::TimeDelta());

  const int index =
      base::ClampFloor((timestamp % duration_) / bucket_duration_);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, num_buckets_);
  return index;
}

}  // namespace ml
}  // namespace power
}  // namespace ash
