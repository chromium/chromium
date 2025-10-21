// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decider.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result>
ScrollJankV4Decider::DecideJankForPresentedFrame(
    base::TimeTicks first_input_generation_ts,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval,
    bool has_inertial_input,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels) {
  CHECK_LE(first_input_generation_ts, last_input_generation_ts);
  CHECK(has_inertial_input || max_abs_inertial_raw_delta_pixels == 0);

  if (presentation_ts <= last_input_generation_ts) {
    // TODO(crbug.com/40913586): Investigate when these edge cases can be
    // triggered in field and web tests. We have already seen this triggered in
    // field, and some web tests where an event with null(0) timestamp gets
    // coalesced with a "normal" input.
    return std::nullopt;
  }

  // TODO(crbug.com/276722271) : Analyze and reduce these cases of out of order
  // frame termination.
  if (prev_frame_data_.has_value() &&
      presentation_ts <= prev_frame_data_->presentation_ts) {
    return std::nullopt;
  }

  ScrollUpdateEventMetrics::ScrollJankV4Result result = {
      .abs_total_raw_delta_pixels = abs_total_raw_delta_pixels,
      .max_abs_inertial_raw_delta_pixels = max_abs_inertial_raw_delta_pixels,
  };

  bool is_janky = false;
  int vsyncs_since_previous_frame;
  if (prev_frame_data_.has_value()) {
    result.running_delivery_cutoff = prev_frame_data_->running_delivery_cutoff;

    // Determine how many VSyncs there have been between the previous and
    // current frame. Sometimes the presentation_delta isn't an exact multiple
    // of `vsync_interval`. We add `(vsync_interval / 2)` to round the result
    // properly to the nearest integer.
    base::TimeDelta presentation_delta =
        presentation_ts - prev_frame_data_->presentation_ts;
    vsyncs_since_previous_frame = std::max<int>(
        (presentation_delta + (vsync_interval / 2)) / vsync_interval, 1);
    result.vsyncs_since_previous_frame = vsyncs_since_previous_frame;

    if (vsyncs_since_previous_frame > 1) {
      // If there was at least one VSync between the previous and current frame,
      // determine whether the current frame should be marked as janky because
      // Chrome should have presented its first input (`earliest_event`) in an
      // earlier VSync based on the rules described in
      // https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA.
      JankReasonArray<int> missed_vsyncs_per_reason =
          CalculateMissedVsyncsPerReason(
              vsyncs_since_previous_frame, first_input_generation_ts,
              presentation_ts, vsync_interval, abs_total_raw_delta_pixels,
              max_abs_inertial_raw_delta_pixels, result);

      // A frame is janky if ANY of the rules decided that Chrome missed one or
      // more VSyncs.
      is_janky = std::any_of(
          missed_vsyncs_per_reason.begin(), missed_vsyncs_per_reason.end(),
          [](int missed_vsyncs) { return missed_vsyncs > 0; });

      result.missed_vsyncs_per_reason = std::move(missed_vsyncs_per_reason);
    }
  }

  // How quickly Chrome was able to deliver input in the current frame?
  base::TimeDelta cur_delivery_cutoff =
      presentation_ts - last_input_generation_ts;
  result.current_delivery_cutoff = cur_delivery_cutoff;
  base::TimeDelta new_running_delivery_cutoff;
  if (prev_frame_data_.has_value() && !is_janky) {
    static const double kDiscountFactor =
        features::kScrollJankV4MetricDiscountFactor.Get();
    base::TimeDelta discounted_prev_delivery_cutoff =
        prev_frame_data_->running_delivery_cutoff +
        vsyncs_since_previous_frame * kDiscountFactor * vsync_interval;
    new_running_delivery_cutoff =
        std::min(discounted_prev_delivery_cutoff, cur_delivery_cutoff);
  } else {
    // If this is the first frame in the scroll, there's no past performance
    // (`prev_frame_data_->running_delivery_cutoff`) to compare against. If
    // we've just marked this frame as janky, forget Chrome's past performance
    // and start from scratch.
    new_running_delivery_cutoff = cur_delivery_cutoff;
  }

  // Finally, update internal state for the next iteration.
  prev_frame_data_ = {
      .presentation_ts = presentation_ts,
      .has_inertial_input = has_inertial_input,
      .abs_total_raw_delta_pixels = abs_total_raw_delta_pixels,
      .running_delivery_cutoff = new_running_delivery_cutoff,
  };

  return result;
}

void ScrollJankV4Decider::OnScrollStarted() {
  Reset();
}

void ScrollJankV4Decider::OnScrollEnded() {
  Reset();
}

void ScrollJankV4Decider::Reset() {
  prev_frame_data_ = std::nullopt;
}

JankReasonArray<int> ScrollJankV4Decider::CalculateMissedVsyncsPerReason(
    int vsyncs_since_previous_frame,
    base::TimeTicks first_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels,
    ScrollUpdateEventMetrics::ScrollJankV4Result& result) const {
  DCHECK_GT(vsyncs_since_previous_frame, 1);

  static const double kStabilityCorrection =
      features::kScrollJankV4MetricStabilityCorrection.Get();
  static const double kDiscountFactor =
      features::kScrollJankV4MetricDiscountFactor.Get();
  static const double kFastScrollContinuityThreshold =
      features::kScrollJankV4MetricFastScrollContinuityThreshold.Get();
  static const double kFlingContinuityThreshold =
      features::kScrollJankV4MetricFlingContinuityThreshold.Get();

  JankReasonArray<int> missed_vsyncs_per_reason = {};

  DCHECK(prev_frame_data_.has_value());
  const PreviousFrameData& prev_frame_data = *prev_frame_data_;

  // Rule 1: Running consistency.
  // Discount `prev_frame_data.running_delivery_cutoff` based on how many VSyncs
  // there have been since the previous frame (to be a bit more lenient) and
  // subtract stability correction (to be a bit more strict). This is what the
  // current VSync would hypothetically have been judged against if it didn't
  // contain any inputs.
  base::TimeDelta adjusted_delivery_cutoff =
      prev_frame_data.running_delivery_cutoff +
      (vsyncs_since_previous_frame - 1) * kDiscountFactor * vsync_interval -
      kStabilityCorrection * vsync_interval;
  result.adjusted_delivery_cutoff = adjusted_delivery_cutoff;
  base::TimeDelta first_input_to_presentation =
      presentation_ts - first_input_generation_ts;
  // Based on Chrome's past performance (`adjusted_delivery_cutoff`), how many
  // VSyncs ago could Chrome have presented the current frame's first input?
  // Note that we divide by `(1 - kDiscountFactor)` because we need to reverse
  // the discounting as we consider earlier VSyncs.
  int missed_vsyncs_due_to_deceleration =
      (first_input_to_presentation - adjusted_delivery_cutoff) /
      ((1 - kDiscountFactor) * vsync_interval);
  if (missed_vsyncs_due_to_deceleration > 0) {
    missed_vsyncs_per_reason[static_cast<int>(
        JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery)] =
        missed_vsyncs_due_to_deceleration;
  }

  // Rules 2 & 3: Fast scroll and fling continuity.
  bool cur_is_sufficiently_fast_fling =
      max_abs_inertial_raw_delta_pixels >= kFlingContinuityThreshold;
  bool cur_is_fast_scroll =
      abs_total_raw_delta_pixels >= kFastScrollContinuityThreshold;
  bool prev_is_fast_scroll = prev_frame_data.abs_total_raw_delta_pixels >=
                             kFastScrollContinuityThreshold;
  if (cur_is_sufficiently_fast_fling) {
    if (prev_frame_data.has_inertial_input) {
      // Chrome missed one or more VSyncs in the middle of a fling.
      missed_vsyncs_per_reason[static_cast<int>(
          JankReason::kMissedVsyncDuringFling)] =
          vsyncs_since_previous_frame - 1;
    } else if (prev_is_fast_scroll) {
      // Chrome missed one or more VSyncs during the transition from a fast
      // regular scroll to a fling.
      missed_vsyncs_per_reason[static_cast<int>(
          JankReason::kMissedVsyncAtStartOfFling)] =
          vsyncs_since_previous_frame - 1;
    }
  } else if (prev_is_fast_scroll && cur_is_fast_scroll) {
    // Chrome missed one or more VSyncs in the middle of a fast regular scroll.
    missed_vsyncs_per_reason[static_cast<int>(
        JankReason::kMissedVsyncDuringFastScroll)] =
        vsyncs_since_previous_frame - 1;
  }

  return missed_vsyncs_per_reason;
}

}  // namespace cc
