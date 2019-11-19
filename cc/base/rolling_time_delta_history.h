// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_ROLLING_TIME_DELTA_HISTORY_H_
#define CC_BASE_ROLLING_TIME_DELTA_HISTORY_H_

#include <stddef.h>

#include <set>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "cc/base/base_export.h"

namespace cc {

// Stores a limited number of samples. When the maximum size is reached, each
// insertion results in the deletion of the oldest remaining sample.
class CC_BASE_EXPORT RollingTimeDeltaHistory {
 public:
  explicit RollingTimeDeltaHistory(size_t max_size);
  RollingTimeDeltaHistory(const RollingTimeDeltaHistory&) = delete;

  ~RollingTimeDeltaHistory();

  RollingTimeDeltaHistory& operator=(const RollingTimeDeltaHistory&) = delete;

  void InsertSample(base::TimeDelta time);
  void RemoveOldestSample();
  size_t sample_count() const { return sample_set_.size(); }

  void Clear();

  // Returns the smallest sample that is greater than or equal to the specified
  // percent of samples. If there aren't any samples, returns base::TimeDelta().
  base::TimeDelta Percentile(double percent) const;

 private:
  typedef std::multiset<base::TimeDelta> TimeDeltaMultiset;

  base::TimeDelta ComputePercentile(double percent) const;

  TimeDeltaMultiset sample_set_;
  base::circular_deque<TimeDeltaMultiset::iterator> chronological_sample_deque_;
  size_t max_size_;

  mutable base::flat_map<double, base::TimeDelta> percentile_cache_;
};

}  // namespace cc

#endif  // CC_BASE_ROLLING_TIME_DELTA_HISTORY_H_
