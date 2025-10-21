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
#include "cc/metrics/scroll_jank_v4_decider.h"

namespace cc {
class ScrollJankUkmReporter;

class CC_EXPORT ScrollJankDroppedFrameTracker {
 public:
  ScrollJankDroppedFrameTracker();
  ~ScrollJankDroppedFrameTracker();

  ScrollJankDroppedFrameTracker(const ScrollJankDroppedFrameTracker&) = delete;

  void ReportLatestPresentationData(ScrollUpdateEventMetrics& earliest_event,
                                    ScrollUpdateEventMetrics& latest_event,
                                    base::TimeTicks last_input_generation_ts,
                                    base::TimeTicks presentation_ts,
                                    base::TimeDelta vsync_interval,
                                    bool has_inertial_input,
                                    float abs_total_raw_delta_pixels,
                                    float max_abs_inertial_raw_delta_pixels);
  void OnScrollStarted();
  void OnScrollEnded();

  void set_scroll_jank_ukm_reporter(
      ScrollJankUkmReporter* scroll_jank_ukm_reporter) {
    scroll_jank_ukm_reporter_ = scroll_jank_ukm_reporter;
  }

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesWindowHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.FixedWindow";
  static constexpr const char* kDelayedFramesWindowV4Histogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow";
  static constexpr const char*
      kMissedVsyncDueToDeceleratingInputFrameDeliveryV4Histogram =
          "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
          "MissedVsyncDueToDeceleratingInputFrameDelivery";
  static constexpr const char* kMissedVsyncDuringFastScrollV4Histogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll";
  static constexpr const char* kMissedVsyncAtStartOfFlingV4Histogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling";
  static constexpr const char* kMissedVsyncDuringFlingV4Histogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFlingV4Histogram";
  static constexpr const char* kDelayedFramesPerScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage.PerScroll";
  static constexpr const char* kDelayedFramesPerScrollV4Histogram =
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll";
  static constexpr const char* kMissedVsyncsSumInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum.FixedWindow";
  static constexpr const char* kMissedVsyncsSumInWindowV4Histogram =
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInWindowV4Histogram =
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsMax.PerScroll";
  static constexpr const char* kMissedVsyncsSumPerScrollHistogram =
      "Event.ScrollJank.MissedVsyncsSum.PerScroll";
  static constexpr const char* kMissedVsyncsPerFrameHistogram =
      "Event.ScrollJank.MissedVsyncs.PerFrame";

 private:
  void EmitPerWindowHistogramsAndResetCounters();
  void EmitPerScrollHistogramsAndResetCounters();
  void EmitPerWindowV4HistogramsAndResetCounters();
  void EmitPerScrollV4HistogramsAndResetCounters();
  void ReportLatestPresentationDataV4(
      ScrollUpdateEventMetrics& earliest_event,
      base::TimeTicks first_input_generation_v4_ts,
      base::TimeTicks last_input_generation_ts,
      base::TimeTicks presentation_ts,
      base::TimeDelta vsync_interval,
      bool has_inertial_input,
      float abs_total_raw_delta_pixels,
      float max_abs_inertial_raw_delta_pixels);
  void UpdateDelayedFrameAndMissedVsyncCountersV4(
      const JankReasonArray<int>& missed_vsyncs_per_reason);

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

  struct JankDataFixedWindowV4 {
    // Total number of frames that Chrome presented.
    int presented_frames = 0;

    // Total number of frames that Chrome didn't present on time, i.e. presented
    // one or more VSyncs later than it should have (for any reason).
    // Must be less than or equal to `presented_frames`.
    int delayed_frames = 0;

    // Number of frames that Chrome didn't present on time for each reason.
    // Each value must be less than or equal to `delayed_frames`.
    JankReasonArray<int> delayed_frames_per_reason = {};

    // Total number of VSyncs that Chrome missed (for any reason). Whenever a
    // frame is missed, it could be delayed by >=1 vsyncs, this helps us track
    // how "long" the janks are.
    // Must be greater than or equal to `delayed_frames`.
    int missed_vsyncs = 0;

    // Maximum number of consecutive VSyncs that Chrome missed (for any reason).
    // Must be less than or equal to `missed_vsyncs`.
    int max_consecutive_missed_vsyncs = 0;
  };

  struct JankDataPerScrollV4 {
    // Total number of frames that Chrome presented.
    int presented_frames = 0;

    // Total number of frames that Chrome didn't present on time, i.e. presented
    // one or more VSyncs later than it should have (for any reason).
    // Must be less than or equal to `presented_frames`.
    int delayed_frames = 0;
  };

  JankData fixed_window_;
  JankDataFixedWindowV4 fixed_window_v4_;
  std::optional<JankData> per_scroll_;
  std::optional<JankDataPerScrollV4> per_scroll_v4_;

  raw_ptr<ScrollJankUkmReporter> scroll_jank_ukm_reporter_ = nullptr;
  ScrollJankV4Decider v4_decider_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_DROPPED_FRAME_TRACKER_H_
