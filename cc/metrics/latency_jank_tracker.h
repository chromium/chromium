// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_LATENCY_JANK_TRACKER_H_
#define CC_METRICS_LATENCY_JANK_TRACKER_H_

#include "base/time/time.h"

namespace cc {

// This is used to track the state of the current scroll and for jank
// reporting, because we need to compare the current frame with the
// previous and next frame duration to detect jank, using the
// frame_time > (prev/next)_frame_time + 0.5 + 1e-9 formula.
class LatencyJankTracker {
 public:
  LatencyJankTracker();
  ~LatencyJankTracker();

  LatencyJankTracker(const LatencyJankTracker&) = delete;
  LatencyJankTracker& operator=(const LatencyJankTracker&) = delete;

  void ReportScrollTimings(base::TimeTicks original_timestamp,
                           base::TimeTicks gpu_swap_end_timestamp,
                           bool first_frame);

 private:
  // Data holder for all intermediate state for jank tracking.
  struct JankTrackerState {
    int total_update_events_ = 0;
    int janky_update_events_ = 0;
    bool prev_scroll_update_reported_ = false;
    base::TimeDelta prev_duration_;
    base::TimeDelta total_update_duration_;
    base::TimeDelta janky_update_duration_;
  };
  JankTrackerState jank_state_;
};

}  // namespace cc

#endif  // CC_METRICS_LATENCY_JANK_TRACKER_H_
