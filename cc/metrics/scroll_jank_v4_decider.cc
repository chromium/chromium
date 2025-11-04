// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decider.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;

}  // namespace

std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result>
ScrollJankV4Decider::DecideJankForFrameWithScrollUpdates(
    base::TimeTicks first_input_generation_ts,
    base::TimeTicks last_input_generation_ts,
    const ScrollDamage& damage,
    const viz::BeginFrameArgs& args,
    bool has_inertial_input,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels) {
  CHECK(has_inertial_input || max_abs_inertial_raw_delta_pixels == 0);

  if (!IsValidFrame(first_input_generation_ts, last_input_generation_ts, damage,
                    args)) {
    return std::nullopt;
  }

  base::TimeDelta vsync_interval = args.interval;
  const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);

  ScrollUpdateEventMetrics::ScrollJankV4Result result = {
      .is_damaging_frame = !!damaging_frame,
      .abs_total_raw_delta_pixels = abs_total_raw_delta_pixels,
      .max_abs_inertial_raw_delta_pixels = max_abs_inertial_raw_delta_pixels,
  };

  bool is_janky = false;
  int vsyncs_since_previous_frame = 0;
  if (prev_frame_data_.has_value()) {
    const std::optional<PreviousFrameData::PresentationData>&
        prev_presentation_data = prev_frame_data_->presentation_data;
    if (prev_presentation_data.has_value()) {
      result.running_delivery_cutoff =
          prev_presentation_data->running_delivery_cutoff;
    }

    // Determine how many VSyncs there have been between the previous and
    // current frame. By default, compare the presentation times. If the current
    // or previous frame's presentation time isn't available, fall back to
    // comparing begin frame times. Sometimes the delta isn't an exact multiple
    // of `vsync_interval`. We add `(vsync_interval / 2)` to round the result
    // properly to the nearest integer.
    base::TimeDelta delta =
        damaging_frame && prev_presentation_data.has_value()
            ? damaging_frame->presentation_ts -
                  prev_presentation_data->presentation_ts
            : args.frame_time - prev_frame_data_->begin_frame_ts;
    vsyncs_since_previous_frame =
        std::max<int>((delta + (vsync_interval / 2)) / vsync_interval, 1);
    result.vsyncs_since_previous_frame = vsyncs_since_previous_frame;

    if (vsyncs_since_previous_frame > 1) {
      // If there was at least one VSync between the previous and current frame,
      // determine whether the current frame should be marked as janky because
      // Chrome should have presented its first input (`earliest_event`) in an
      // earlier VSync based on the rules described in
      // https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA.
      JankReasonArray<int> missed_vsyncs_per_reason =
          CalculateMissedVsyncsPerReason(
              vsyncs_since_previous_frame, first_input_generation_ts, damage,
              vsync_interval, abs_total_raw_delta_pixels,
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
  std::optional<PreviousFrameData::PresentationData> presentation_data =
      CalculatePresentationData(vsyncs_since_previous_frame, is_janky,
                                last_input_generation_ts, damage, args, result);

  // Finally, update internal state for the next iteration.
  prev_frame_data_ = {
      .has_inertial_input = has_inertial_input,
      .abs_total_raw_delta_pixels = abs_total_raw_delta_pixels,
      .begin_frame_ts = args.frame_time,
      .presentation_data = presentation_data,
  };

  return result;
}

void ScrollJankV4Decider::OnScrollStarted() {
  Reset();
}

void ScrollJankV4Decider::OnScrollEnded() {
  Reset();
}

bool ScrollJankV4Decider::IsValidFrame(
    base::TimeTicks first_input_generation_ts,
    base::TimeTicks last_input_generation_ts,
    const ScrollDamage& damage,
    const viz::BeginFrameArgs& args) const {
  if (last_input_generation_ts < first_input_generation_ts) {
    return false;
  }

  const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);
  if (damaging_frame &&
      damaging_frame->presentation_ts <= last_input_generation_ts) {
    // TODO(crbug.com/40913586): Investigate when these edge cases can be
    // triggered in field and web tests. We have already seen this triggered in
    // field, and some web tests where an event with null(0) timestamp gets
    // coalesced with a "normal" input.
    return false;
  }

  if (!prev_frame_data_.has_value()) {
    // If this is the first frame, then there's nothing left to check.
    return true;
  }

  // TODO(crbug.com/276722271) : Analyze and reduce these cases of out of order
  // frame termination.
  if (damaging_frame && prev_frame_data_->presentation_data.has_value()) {
    // If we have presentation timestamps for both the previous and current
    // frame, compare them.
    return damaging_frame->presentation_ts >
           prev_frame_data_->presentation_data->presentation_ts;
  } else {
    // If not, compare their begin frame timestamps.
    return args.frame_time > prev_frame_data_->begin_frame_ts;
  }
}

JankReasonArray<int> ScrollJankV4Decider::CalculateMissedVsyncsPerReason(
    int vsyncs_since_previous_frame,
    base::TimeTicks first_input_generation_ts,
    const ScrollDamage& damage,
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
  // Discount `prev_frame_data.presentation_data->running_delivery_cutoff` based
  // on how many VSyncs there have been since the previous frame (to be a bit
  // more lenient) and subtract stability correction (to be a bit more strict).
  // This is what the current VSync would hypothetically have been judged
  // against if it didn't contain any inputs.
  if (const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);
      damaging_frame && prev_frame_data.presentation_data.has_value()) {
    base::TimeDelta adjusted_delivery_cutoff =
        prev_frame_data.presentation_data->running_delivery_cutoff +
        (vsyncs_since_previous_frame - 1) * kDiscountFactor * vsync_interval -
        kStabilityCorrection * vsync_interval;
    result.adjusted_delivery_cutoff = adjusted_delivery_cutoff;
    base::TimeDelta first_input_to_presentation =
        damaging_frame->presentation_ts - first_input_generation_ts;
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

std::optional<ScrollJankV4Decider::PreviousFrameData::PresentationData>
ScrollJankV4Decider::CalculatePresentationData(
    int vsyncs_since_previous_frame,
    bool is_janky,
    base::TimeTicks last_input_generation_ts,
    const ScrollDamage& damage,
    const viz::BeginFrameArgs& args,
    ScrollUpdateEventMetrics::ScrollJankV4Result& result) const {
  // We should consider Chrome's past performance
  // (`prev_frame_data_->presentation_data->running_delivery_cutoff`) to update
  // the running delivery cut-off as long as there's data available for the
  // previous frame and the current frame is not janky. If there's no data
  // available for the previous frame (because the current frame is the first
  // damaging frame in the scroll or the first damaging frame since the decider
  // marked a non-damaging frame as jank), we start from scratch. Alternatively,
  // if we've just marked the current frame as janky, forget Chrome's past
  // performance and start from scratch.
  std::optional<base::TimeDelta> discounted_prev_delivery_cutoff =
      [&]() -> std::optional<base::TimeDelta> {
    bool should_consider_prev_frame_cutoff =
        prev_frame_data_.has_value() &&
        prev_frame_data_->presentation_data.has_value() && !is_janky;
    if (!should_consider_prev_frame_cutoff) {
      return std::nullopt;
    }
    static const double kDiscountFactor =
        features::kScrollJankV4MetricDiscountFactor.Get();
    return prev_frame_data_->presentation_data->running_delivery_cutoff +
           vsyncs_since_previous_frame * kDiscountFactor * args.interval;
  }();

  if (const DamagingFrame* damaging_frame =
          std::get_if<DamagingFrame>(&damage)) {
    base::TimeDelta cur_delivery_cutoff =
        damaging_frame->presentation_ts - last_input_generation_ts;
    result.current_delivery_cutoff = cur_delivery_cutoff;
    base::TimeDelta new_running_delivery_cutoff =
        discounted_prev_delivery_cutoff.has_value()
            ? std::min(*discounted_prev_delivery_cutoff, cur_delivery_cutoff)
            : cur_delivery_cutoff;
    return PreviousFrameData::PresentationData{
        .presentation_ts = damaging_frame->presentation_ts,
        .running_delivery_cutoff = new_running_delivery_cutoff,
    };
  }

  if (discounted_prev_delivery_cutoff.has_value()) {
    // If this is a non-damaging frame that's not janky, we pretend as if it
    // was presented consistently, i.e. we assume that it has the same
    // duration between its begin frame and presentation timestamps as the
    // most recent damaging frame.
    base::TimeTicks extrapolated_presentation_ts =
        prev_frame_data_->presentation_data->presentation_ts +
        (args.frame_time - prev_frame_data_->begin_frame_ts);
    // We don't know whether Chrome would have been actually able to deliver
    // the non-damaging inputs at `extrapolated_presentation_ts`, so we don't
    // calculate the current frame's delivery cut-off. Instead, we keep
    // discounting the previous frame's delivery cut-off.
    return PreviousFrameData::PresentationData{
        .presentation_ts = extrapolated_presentation_ts,
        .running_delivery_cutoff = *discounted_prev_delivery_cutoff,
    };
  }

  // If the decider hasn't received any damaging frames since the beginning
  // of the scroll or since the most recent non-damaging frame that the
  // decider marked as janky, then we cannot extrapolate Chrome's past
  // performance to the current non-damaging frame. The same argument applies if
  // the current non-damaging frame is janky.
  return std::nullopt;
}

void ScrollJankV4Decider::Reset() {
  prev_frame_data_ = std::nullopt;
}

}  // namespace cc
