// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
#define CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_

#include <optional>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_ukm_reporter.h"

namespace cc {
class ScrollJankUkmReporter;

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

  void set_scroll_jank_ukm_reporter(
      ScrollJankUkmReporter* scroll_jank_ukm_reporter) {
    scroll_jank_ukm_reporter_ = scroll_jank_ukm_reporter;
  }

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesWindowHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.FixedWindow";
  static constexpr const char* kMissedVsyncsWindowHistogram =
      "Event.ScrollJank.MissedVsyncsPercentage.FixedWindow";
  static constexpr const char* kDelayedFramesPerScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.PerScroll";
  static constexpr const char* kMissedVsyncsPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsPercentage.PerScroll";
  static constexpr const char* kMissedVsyncsSumInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum.FixedWindow";
  static constexpr const char* kMissedVsyncsSumInVsyncWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum.FixedWindow2";
  static constexpr const char* kMissedVsyncsMaxInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInVsyncWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax.FixedWindow2";
  static constexpr const char* kMissedVsyncsMaxPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsMax.PerScroll";
  static constexpr const char* kMissedVsyncsSumPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsSum.PerScroll";
  static constexpr const char* kMissedVsyncsPerFrameHistogram =
      "Event.ScrollJank.MissedVsyncs.PerFrame";

 private:
  void EmitPerWindowHistogramsAndResetCounters();
  void EmitPerScrollHistogramsAndResetCounters();
  void EmitPerVsyncWindowHistogramsAndResetCounters();
  void EmitPerScrollVsyncHistogramsAndResetCounters();

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
    int num_past_vsyncs = 0;
  };

  JankData fixed_window_;
  // TODO(b/306611560): Cleanup experimental per vsync metric or promote to
  // default.
  JankData experimental_vsync_fixed_window_;
  std::optional<JankData> per_scroll_;
  std::optional<JankData> experimental_per_scroll_vsync_;

  raw_ptr<ScrollJankUkmReporter> scroll_jank_ukm_reporter_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
