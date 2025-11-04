// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
#define CC_METRICS_SCROLL_JANK_V4_DECIDER_H_

#include <optional>
#include <variant>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

// Class responsible for deciding whether a frame containing one or more scroll
// updates was janky or not according to the scroll jank v4 metric. In order to
// work correctly, it must be informed about each frame that contained one or
// more scroll updates in chronological order.
//
// To avoid false positives, the decider must even be informed about
// non-damaging scroll updates and frames. See
// `ScrollUpdateEventMetrics::ScrollJankV4Result::is_damaging_frame` for the
// definition of non-damaging scroll updates and frames.
//
// See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details about the scroll jank v4 metric.
class CC_EXPORT ScrollJankV4Decider {
 public:
  // Decides whether a frame which contains scroll updates was janky based on
  // the following information:
  //
  //   * `first_input_generation_ts` and `last_input_generation_ts`: The
  //     generation timestamp of the first and last scroll update included
  //     (coalesced) in the frame.
  //      updates included (coalesced) in the frame.
  //   * `damage`: Information about a frame's scroll damage. For damaging
  //     frames, `DamagingFrame::presentation_ts` specifies when the frame was
  //     presented to the user.
  //   * `args`: The presented frame's arguments (especially `args.interval`).
  //   * `has_inertial_input`: Whether at least one of the scroll updates in the
  //     frame was inertial.
  //   * `abs_total_raw_delta_pixels`: The absolute value of the total raw delta
  //     (`ScrollUpdateEventMetrics::delta()`) of all scroll updates included in
  //     the frame.
  //   * `max_abs_inertial_raw_delta_pixels`: The maximum absolute value of raw
  //     delta (`ScrollUpdateEventMetrics::delta()`) over all inertial scroll
  //     updates included in the frame.
  //
  // This method treats non-damaging frames as if Chrome successfully presented
  // them on time, even if Chrome ended up not presenting the frames or they
  // were dropped/throttled/delayed. Rationale: If a frame is non-damaging, the
  // user can't tell whether Chrome presented the frame on time (or even whether
  // Chrome presented the frame at all).
  //
  // Returns an empty optional if the frame is malformed in some way (e.g. it
  // has an earlier presentation time than the previous frame provided to the
  // decider).
  std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result>
  DecideJankForFrameWithScrollUpdates(
      base::TimeTicks first_input_generation_ts,
      base::TimeTicks last_input_generation_ts,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const viz::BeginFrameArgs& args,
      bool has_inertial_input,
      float abs_total_raw_delta_pixels,
      float max_abs_inertial_raw_delta_pixels);

  void OnScrollStarted();
  void OnScrollEnded();

 private:
  // Information about the previous frame relevant for the scroll jank v4
  // metric.
  struct PreviousFrameData {
    // Whether the previous frame contained an inertial input (i.e. was it a
    // fling).
    bool has_inertial_input;

    // The absolute total raw (unpredicted) delta of all inputs included in the
    // previous frame (in pixels).
    float abs_total_raw_delta_pixels;

    // The time at which the frame started. See
    // `viz::BeginFrameArgs::frame_time`.
    base::TimeTicks begin_frame_ts;

    struct PresentationData {
      // When the previous frame was presented to the user.
      //
      // If the previous frame was non-damaging, this value is instead
      // extrapolated from the most recently presented damaging frame (i.e. we
      // assume a constant duration between `begin_frame_ts` and
      // `presentation_ts`):
      //
      // ```
      // non_damaging_frame.presentation_data.presentation_ts
      //   = non_damaging_frame.begin_frame_ts
      //   + (presented_damaging_frame.presentation_data.presentation_ts
      //        - presented_damaging_frame.begin_frame_ts)
      // ```
      base::TimeTicks presentation_ts;

      // The running delivery cut-off. At a high-level, this value represents
      // how quickly Chrome was previously able to present inputs (weighted
      // towards recent frames). If Chrome misses a VSync, the decider will
      // judge the subsequent frame (i.e. determine whether the frame should
      // be marked as janky) against this value. This value equals:
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
      //   * `i = 1` corresponds a presented damaging frame as follows:
      //       * If the frame that the decider most recently marked as janky was
      //         damaging, `i = 1` corresponds to that janky frame.
      //       * If the frame that the decider most recently marked as janky was
      //         non-damaging, `i = 1` corresponds to the first damaging frame
      //         that the decider processed after that janky frame.
      //       * If the decider hasn't marked any frame in this scroll as janky,
      //         `i = 1` corresponds to the first damaging frame within the
      //         current scroll.
      //   * `i = N` corresponds to the frame (damaging or non-damaging) that
      //     the decider has most recently processed.
      //   * `presentation_ts[i]` and `last_input_generation_ts[i]` refer to:
      //       * If the i-th frame was a damaging frame, they refer to the
      //         values supplied to the i-th
      //         `DecideJankForPresentedDamagingFrame()` call.
      //       * If the i-th frame was a non-damaging frame, they refer to the
      //         values supplied to the j-th
      //         `DecideJankForPresentedDamagingFrame()` call where j was the
      //         most recent damaging frame before i (we assume a constant
      //         duration between `last_input_generation_ts` and
      //         `presentation_ts`).
      //   * `VsyncsBetween(i, N)` is approximately:
      //
      //     ```
      //     (presentation_ts[N] - presentation_ts[i] + (vsync_interval / 2))
      //       / vsync_interval
      //     ```
      //
      //     Approximation for non-damaging frames:
      //
      //     ```
      //     (begin_frame_ts[N] - begin_frame_ts[i] + (vsync_interval / 2))
      //       / vsync_interval
      //     ```
      base::TimeDelta running_delivery_cutoff;
    };

    // The documentation of `prev_frame_data_` below explains when this field is
    // non-empty.
    std::optional<PresentationData> presentation_data;
  };

  void Reset();

  bool IsValidFrame(base::TimeTicks first_input_generation_ts,
                    base::TimeTicks last_input_generation_ts,
                    const ScrollJankV4Frame::ScrollDamage& damage,
                    const viz::BeginFrameArgs& args) const;

  JankReasonArray<int> CalculateMissedVsyncsPerReason(
      int vsyncs_since_previous_frame,
      base::TimeTicks first_input_generation_ts,
      const ScrollJankV4Frame::ScrollDamage& damage,
      base::TimeDelta vsync_interval,
      float abs_total_raw_delta_pixels,
      float max_abs_inertial_raw_delta_pixels,
      ScrollUpdateEventMetrics::ScrollJankV4Result& result) const;

  std::optional<PreviousFrameData::PresentationData> CalculatePresentationData(
      int vsyncs_since_previous_frame,
      bool is_janky,
      base::TimeTicks last_input_generation_ts,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const viz::BeginFrameArgs& args,
      ScrollUpdateEventMetrics::ScrollJankV4Result& result) const;

  // Information about the previous frame, which can be in three states (2A and
  // 2B are different conditions for the same state):
  //
  //   1.  If the decider hasn't been informed about any frames (damaging or
  //       non-damaging) since the beginning of the current scroll (i.e. neither
  //       `DecideJankForPresentedDamagingFrame()` nor
  //       `DecideJankForNonDamagingFrame()` has been called since the last call
  //       to either `OnScrollStarted()` or `OnScrollEnded()`), then
  //       `prev_frame_data_` is empty.
  //   2A. If the decider has only been informed about non-damaging frames since
  //       the beginning of the current scroll (i.e. only
  //       `DecideJankForNonDamagingFrame()` has been called since the last call
  //       to either `OnScrollStarted()` or `OnScrollEnded()`), then
  //       `prev_frame_data_` has a value but
  //       `prev_frame_data_.presentation_data` is empty.
  //   2B. If the decider marked a non-damaging frame as janky and it has only
  //       been informed about non-damaging frames since then (i.e. only
  //       `DecideJankForNonDamagingFrame()` has been called since
  //       `DecideJankForNonDamagingFrame()` returned a janky result), then
  //       `prev_frame_data_` has a value but
  //       `prev_frame_data_.presentation_data` is empty.
  //   3.  Otherwise, both `prev_frame_data` and
  //       `prev_frame_data_.presentation_data` have values.
  //
  // The state has the following practical implications for the decider's
  // behavior on the next frame:
  //
  //   * If `prev_frame_data_` is empty, then there's no information about the
  //     previous frame, so the decider will definitely NOT mark the next frame
  //     as janky.
  //   * If `prev_frame_data_` has a value but
  //     `prev_frame_data_.presentation_data`
  //     is empty, then the decider cannot evaluate Chrome's input→frame
  //     delivery, so it will definitely NOT mark the next frame as janky due to
  //     `JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery`. However,
  //     the decider MIGHT still mark the next frame as janky for any other
  //     `JankReason`.
  //   * If both `prev_frame_data` and `prev_frame_data_.presentation_data` have
  //     values, the decider MIGHT mark the next frame as janky for any
  //     `JankReason`.
  std::optional<PreviousFrameData> prev_frame_data_ = std::nullopt;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
