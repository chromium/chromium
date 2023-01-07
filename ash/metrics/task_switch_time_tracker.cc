// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_time_tracker.h"

#include "base/metrics/histogram.h"
#include "base/time/default_tick_clock.h"

namespace ash {

namespace {

// The number of buckets in the histogram.
// See IMPORTANT note below if you want to change this value!
const size_t kBucketCount = 50;

// The underflow (aka minimum) bucket size for the histogram.
// See IMPORTANT note below if you want to change this value!
const int kMinBucketSizeInSeconds = 0;

// The overflow (aka maximium) bucket size for the histogram.
// See IMPORTANT note below if you want to change this value!
const int kMaxBucketSizeInSeconds = 60 * 60;

}  // namespace

TaskSwitchTimeTracker::TaskSwitchTimeTracker(const std::string& histogram_name)
    : TaskSwitchTimeTracker(histogram_name,
                            base::DefaultTickClock::GetInstance()) {}

TaskSwitchTimeTracker::TaskSwitchTimeTracker(const std::string& histogram_name,
                                             const base::TickClock* tick_clock)
    : histogram_name_(histogram_name), tick_clock_(tick_clock) {}

TaskSwitchTimeTracker::~TaskSwitchTimeTracker() = default;

void TaskSwitchTimeTracker::OnTaskSwitch() {
  if (!HasLastActionTime())
    SetLastActionTime();
  else
    RecordTimeDelta();
}

bool TaskSwitchTimeTracker::HasLastActionTime() const {
  return last_action_time_ != base::TimeTicks();
}

base::TimeTicks TaskSwitchTimeTracker::SetLastActionTime() {
  base::TimeTicks previous_last_action_time = last_action_time_;
  last_action_time_ = tick_clock_->NowTicks();
  return previous_last_action_time;
}

void TaskSwitchTimeTracker::RecordTimeDelta() {
  base::TimeTicks previous_last_action_time = SetLastActionTime();
  base::TimeDelta time_delta = last_action_time_ - previous_last_action_time;

  CHECK_GE(time_delta, base::TimeDelta());

  GetHistogram()->Add(time_delta.InSeconds());
}

base::HistogramBase* TaskSwitchTimeTracker::GetHistogram() {
  if (!histogram_) {
    // IMPORTANT: If you change the type of histogram or any values that define
    // its bucket construction then you must rename all of the histograms using
    // the TaskSwitchTimeTracker mechanism.
    histogram_ = base::Histogram::FactoryGet(
        histogram_name_, base::Seconds(kMinBucketSizeInSeconds).InSeconds(),
        base::Seconds(kMaxBucketSizeInSeconds).InSeconds(), kBucketCount,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }

#if DCHECK_IS_ON()
  histogram_->CheckName(histogram_name_);
#endif  // DCHECK_IS_ON()

  return histogram_;
}

}  // namespace ash
