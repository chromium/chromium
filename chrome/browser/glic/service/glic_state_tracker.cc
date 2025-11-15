// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_state_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace glic {

GlicStateTracker::GlicStateTracker(bool initial_state,
                                   const char* histogram_name)
    : state_(initial_state),
      last_change_time_(base::TimeTicks::Now()),
      histogram_name_(histogram_name) {}

GlicStateTracker::~GlicStateTracker() = default;

void GlicStateTracker::OnStateChanged(bool new_state) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (new_state == state_) {
    return;
  }
  if (state_) {
    base::TimeDelta duration = now - last_change_time_;
    total_duration_ += duration;
    if (histogram_name_) {
      base::UmaHistogramCustomTimes(histogram_name_, duration,
                                    base::Milliseconds(1), base::Hours(1), 50);
    }
  }
  state_ = new_state;
  last_change_time_ = now;
}

void GlicStateTracker::Finalize() {
  if (state_) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta duration = now - last_change_time_;
    total_duration_ += duration;
    if (histogram_name_) {
      base::UmaHistogramCustomTimes(histogram_name_, duration,
                                    base::Milliseconds(1), base::Hours(1), 50);
    }
    state_ = false;
    last_change_time_ = now;
  }
}

}  // namespace glic
