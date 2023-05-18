// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
#define CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT ScrollJankDroppedFrameTracker {
 public:
  ScrollJankDroppedFrameTracker();
  ~ScrollJankDroppedFrameTracker();

  ScrollJankDroppedFrameTracker(const ScrollJankDroppedFrameTracker&) = delete;

  void ReportLatestPresentationData(base::TimeTicks first_input_generation_ts,
                                    base::TimeTicks last_input_generation_ts,
                                    base::TimeTicks presentation_ts,
                                    base::TimeDelta vsync_interval);

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesHistogram =
      "Event.Jank.DelayedFramesPercentage";
  static constexpr const char* kMissedVsyncsHistogram =
      "Event.Jank.MissedVsyncCount";

 private:
  void EmitHistogramsAndResetCounters();

  // We could have two different frames with same presentation time and due to
  // this just having previous frame's data is not enough for calculating the
  // metric.
  base::TimeTicks prev_presentation_ts_;
  base::TimeTicks prev_last_input_generation_ts_;

  // Number of frames which were deemed janky.
  int missed_frames_ = 0;
  // Number of vsyncs the frames were delayed by. Whenever a frame is missed it
  // could be delayed >=1 vsyncs, this helps us track how "long" the janks
  // are.
  int missed_vsyncs_ = 0;

  // Not initializing with 0 because the first frame in first window will be
  // always deemed non-janky which makes the metric slightly biased. Setting
  // it to -1 essentially ignores first frame.
  int num_presented_frames_ = -1;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
