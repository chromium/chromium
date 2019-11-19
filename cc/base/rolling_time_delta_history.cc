// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cmath>

#include "cc/base/rolling_time_delta_history.h"

namespace cc {

RollingTimeDeltaHistory::RollingTimeDeltaHistory(size_t max_size)
    : max_size_(max_size) {}

RollingTimeDeltaHistory::~RollingTimeDeltaHistory() = default;

void RollingTimeDeltaHistory::InsertSample(base::TimeDelta time) {
  if (max_size_ == 0)
    return;

  if (sample_set_.size() == max_size_) {
    sample_set_.erase(chronological_sample_deque_.front());
    chronological_sample_deque_.pop_front();
  }

  auto it = sample_set_.insert(time);
  chronological_sample_deque_.push_back(it);
  percentile_cache_.clear();
}

void RollingTimeDeltaHistory::RemoveOldestSample() {
  if (sample_set_.size() > 0) {
    sample_set_.erase(chronological_sample_deque_.front());
    chronological_sample_deque_.pop_front();
  }
}

void RollingTimeDeltaHistory::Clear() {
  chronological_sample_deque_.clear();
  sample_set_.clear();
  percentile_cache_.clear();
}

base::TimeDelta RollingTimeDeltaHistory::Percentile(double percent) const {
  auto pair =
      percentile_cache_.insert(std::make_pair(percent, base::TimeDelta()));
  auto it = pair.first;
  bool inserted = pair.second;
  if (inserted)
    it->second = ComputePercentile(percent);
  return it->second;
}

base::TimeDelta RollingTimeDeltaHistory::ComputePercentile(
    double percent) const {
  if (sample_set_.size() == 0)
    return base::TimeDelta();

  double fraction = percent / 100.0;

  if (fraction <= 0.0)
    return *(sample_set_.begin());

  if (fraction >= 1.0)
    return *(sample_set_.rbegin());

  size_t num_smaller_samples =
      static_cast<size_t>(std::ceil(fraction * sample_set_.size())) - 1;

  if (num_smaller_samples > sample_set_.size() / 2) {
    size_t num_larger_samples = sample_set_.size() - num_smaller_samples - 1;
    auto it = sample_set_.rbegin();
    for (size_t i = 0; i < num_larger_samples; i++)
      it++;
    return *it;
  }

  auto it = sample_set_.begin();
  for (size_t i = 0; i < num_smaller_samples; i++)
    it++;
  return *it;
}

}  // namespace cc
