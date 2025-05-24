// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/periodic_sampling_scheduler.h"

#include "base/rand_util.h"

namespace base {

PeriodicSamplingScheduler::PeriodicSamplingScheduler(
    TimeDelta sampling_duration,
    double fraction_of_execution_time_to_sample,
    TimeTicks start_time)
    : period_duration_(sampling_duration /
                       fraction_of_execution_time_to_sample),
      sampling_duration_(sampling_duration),
      period_start_time_(start_time) {
  DCHECK(sampling_duration_ <= period_duration_);
}

PeriodicSamplingScheduler::~PeriodicSamplingScheduler() = default;

TimeDelta PeriodicSamplingScheduler::GetTimeToNextCollection() {
  const TimeTicks now = Now();
  // Avoid scheduling in the past in the presence of discontinuous jumps in
  // the current TimeTicks.
  period_start_time_ = std::max(period_start_time_, now);

  const TimeDelta sampling_offset =
      (period_duration_ - sampling_duration_) * RandDouble();
  const TimeTicks next_collection_time = period_start_time_ + sampling_offset;
  period_start_time_ += period_duration_;
  return next_collection_time - now;
}

double PeriodicSamplingScheduler::RandDouble() const {
  return base::RandDouble();
}

TimeTicks PeriodicSamplingScheduler::Now() const {
  return TimeTicks::Now();
}

}  // namespace base
