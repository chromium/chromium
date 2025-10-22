// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_
#define CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_

#include <optional>

#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Class responsible for emitting UMA histograms for the scroll jank v4 metric.
// It emits the following histograms after each window of
// `kHistogramEmitFrequency` presented frames:
//
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDueToDeceleratingInputFrameDelivery
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDuringFastScroll
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncAtStartOfFling
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDuringFling
//   * Event.ScrollJank.MissedVsyncsSum4.FixedWindow
//   * Event.ScrollJank.MissedVsyncsMax4.FixedWindow
//
// and the following histograms after each scroll:
//
//   * Event.ScrollJank.DelayedFramesPercentage4.PerScroll
class CC_EXPORT ScrollJankV4HistogramEmitter {
 public:
  ScrollJankV4HistogramEmitter();
  ~ScrollJankV4HistogramEmitter();
  void OnFramePresented(const JankReasonArray<int>& missed_vsyncs_per_reason);
  void OnScrollStarted();
  void OnScrollEnded();

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesWindowHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow";
  static constexpr const char*
      kMissedVsyncDueToDeceleratingInputFrameDeliveryHistogram =
          "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
          "MissedVsyncDueToDeceleratingInputFrameDelivery";
  static constexpr const char* kMissedVsyncDuringFastScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll";
  static constexpr const char* kMissedVsyncAtStartOfFlingHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling";
  static constexpr const char* kMissedVsyncDuringFlingHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling";
  static constexpr const char* kDelayedFramesPerScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll";
  static constexpr const char* kMissedVsyncsSumInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow";

 private:
  void UpdateCountersForPresentedFrame(
      const JankReasonArray<int>& missed_vsyncs_per_reason);
  void EmitPerWindowHistogramsAndResetCounters();
  void EmitPerScrollHistogramsAndResetCounters();

  struct JankDataFixedWindow {
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

  struct JankDataPerScroll {
    // Total number of frames that Chrome presented.
    int presented_frames = 0;

    // Total number of frames that Chrome didn't present on time, i.e. presented
    // one or more VSyncs later than it should have (for any reason).
    // Must be less than or equal to `presented_frames`.
    int delayed_frames = 0;
  };

  JankDataFixedWindow fixed_window_;
  std::optional<JankDataPerScroll> per_scroll_ = std::nullopt;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_
