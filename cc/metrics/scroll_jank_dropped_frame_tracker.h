// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
#define CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc {

class CC_EXPORT ScrollJankDroppedFrameTracker {
 public:
  ScrollJankDroppedFrameTracker();
  ~ScrollJankDroppedFrameTracker();

  ScrollJankDroppedFrameTracker(const ScrollJankDroppedFrameTracker&) = delete;

  void ReportLatestPresentationData(ScrollUpdateEventMetrics& earliest_event,
                                    base::TimeTicks last_input_generation_ts,
                                    base::TimeTicks presentation_ts,
                                    base::TimeDelta vsync_interval);
  void OnScrollStarted();

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesWindowHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.FixedWindow";
  static constexpr const char* kDelayedFramesPerScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.PerScroll";
  static constexpr const char* kMissedVsyncsSumInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsMax.PerScroll";
  static constexpr const char* kMissedVsyncsSumPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsSum.PerScroll";
  static constexpr const char* kMissedVsyncsPerFrameHistogram =
      "Event.ScrollJank.MissedVsyncs.PerFrame";

 private:
  void EmitPerWindowHistogramsAndResetCounters();
  void EmitPerScrollHistogramsAndResetCounters();

  // We could have two different frames with same presentation time and due to
  // this just having previous frame's data is not enough for calculating the
  // metric.
  base::TimeTicks prev_presentation_ts_;
  base::TimeTicks prev_last_input_generation_ts_;

  struct JankData {
    // Number of frames which were deemed janky.
    int missed_frames = 0;
    // Number of vsyncs the frames were delayed by. Whenever a frame is missed
    // it could be delayed >=1 vsyncs, this helps us track how "long" the janks
    // are.
    int missed_vsyncs = 0;
    int max_missed_vsyncs = 0;
    int num_presented_frames = 0;
  };

  JankData fixed_window_;
  absl::optional<JankData> per_scroll_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
