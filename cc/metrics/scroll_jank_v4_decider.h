// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
#define CC_METRICS_SCROLL_JANK_V4_DECIDER_H_

#include <optional>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Class responsible for deciding whether a presented frame was janky or not
// according to the scroll jank v4 metric. In order to work correctly, it must
// be informed about each presented frame in chronological order.
//
// See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details about the scroll jank v4 metric.
class CC_EXPORT ScrollJankV4Decider {
 public:
  // Decides whether a presented frame was janky based on the following
  // information:
  //
  //   * `first_input_generation_ts` and `last_input_generation_ts`: The
  //   generation timestamp of the first and last scroll update included
  //   (coalesced) in the frame.
  //      updates included (coalesced) in the frame.
  //   * `presentation_ts`: When the frame was presented to the user.
  //   * `vsync_interval`: The current interval between consecutive VSyncs.
  //   * `has_inertial_input`: Whether at least one of the scroll updates in the
  //     frame was inertial.
  //   * `abs_total_raw_delta_pixels`: The absolute value of the total raw delta
  //     (`ScrollUpdateEventMetrics::delta()`) of all scroll updates included in
  //     the frame.
  //   * `max_abs_inertial_raw_delta_pixels`: The maximum absolute value of raw
  //     delta (`ScrollUpdateEventMetrics::delta()`) over all inertial scroll
  //     updates included in the frame.
  //
  // Returns an empty optional if the frame is malformed in some way (e.g. it
  // has an earlier presentation time than the previous frame provided to the
  // decider).
  std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result>
  DecideJankForPresentedFrame(base::TimeTicks first_input_generation_ts,
                              base::TimeTicks last_input_generation_ts,
                              base::TimeTicks presentation_ts,
                              base::TimeDelta vsync_interval,
                              bool has_inertial_input,
                              float abs_total_raw_delta_pixels,
                              float max_abs_inertial_raw_delta_pixels);

  void OnScrollStarted();
  void OnScrollEnded();

 private:
  void Reset();
  JankReasonArray<int> CalculateMissedVsyncsPerReason(
      int vsyncs_since_previous_frame,
      base::TimeTicks first_input_generation_ts,
      base::TimeTicks presentation_ts,
      base::TimeDelta vsync_interval,
      float abs_total_raw_delta_pixels,
      float max_abs_inertial_raw_delta_pixels,
      ScrollUpdateEventMetrics::ScrollJankV4Result& result) const;

  // Information about the previous frame relevant for the scroll jank v4
  // metric.
  struct PreviousFrameData {
    // When the previous frame was presented to the user.
    base::TimeTicks presentation_ts;

    // Whether the previous frame contained an inertial input (i.e. was it a
    // fling).
    bool has_inertial_input;

    // The absolute total raw (unpredicted) delta of all inputs included in the
    // previous frame (in pixels).
    float abs_total_raw_delta_pixels;

    // The running delivery cut-off. At a high-level, this value represents how
    // quickly Chrome was previously able to present inputs (weighted towards
    // recent frames). If Chrome misses a VSync, the scroll jank v4 metric will
    // judge the subsequent frame (i.e. determine whether the frame should be
    // marked as janky) against this value. This value equals:
    //
    // ```
    // min_{i from 1 to N} (
    //   presentation_ts[i]
    //     - last_input_generation_ts[i]
    //     + (
    //         VsyncsBetween(i, N)
    //           * features::kScrollJankV4MetricDiscountFactor.Get()
    //           * vsync_interval
    //       )
    // )
    // ```
    //
    // where:
    //
    //   * `i = 1` corresponds to the frame that the scroll jank v4 metric
    //     (`DecideJankForPresentedFrame()`) has most recently marked as
    //     janky (or the first frame in the current scroll if the metric hasn't
    //     marked any frame in this scroll as janky).
    //   * `i = N` corresponds to the frame that the scroll jank v4 metric
    //     (`DecideJankForPresentedFrame()`) has most recently processed.
    //   * `presentation_ts[i]` and `last_input_generation_ts[i]` refer to the
    //     values supplied to previous `DecideJankForPresentedFrame()` calls.
    //   * `VsyncsBetween(i, N)` is approximately:
    //
    //     ```
    //     (presentation_ts[N] - presentation_s[i] + (vsync_interval / 2))
    //       / vsync_interval
    //     ```
    base::TimeDelta running_delivery_cutoff;
  };

  // Empty if no frames have been presented in the current scroll yet
  // (i.e. `DecideJankForPresentedFrame()` hasn't been called since the
  // last `Reset()` call).
  std::optional<PreviousFrameData> prev_frame_data_ = std::nullopt;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
