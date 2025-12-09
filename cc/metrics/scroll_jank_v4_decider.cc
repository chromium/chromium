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
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;

}  // namespace

ScrollJankV4Result ScrollJankV4Decider::DecideJankForFrameWithRealScrollUpdates(
    const ScrollJankV4FrameStage::ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  CHECK(updates.real().has_value());
  return DecideJankForFrameWithScrollUpdates(updates, damage, args,
                                             IsFastScroll(*updates.real()));
}

ScrollJankV4Result
ScrollJankV4Decider::DecideJankForFrameWithSyntheticScrollUpdatesOnly(
    const ScrollJankV4FrameStage::ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
    bool future_real_frame_is_fast_scroll_or_sufficiently_fast_fling) {
  CHECK(!updates.real().has_value());
  return DecideJankForFrameWithScrollUpdates(
      updates, damage, args,
      future_real_frame_is_fast_scroll_or_sufficiently_fast_fling);
}

ScrollJankV4Result ScrollJankV4Decider::DecideJankForFrameWithScrollUpdates(
    const ScrollUpdates& updates,
    const ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
    bool treat_as_fast_scroll) {
  DCHECK(IsValidFrame(updates, damage, args));
  DCHECK(!prev_frame_data_.has_value() ||
         args.frame_time > prev_frame_data_->begin_frame_ts);

  base::TimeDelta vsync_interval = args.interval;
  const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);

  auto first_scroll_update = GetFirstScrollUpdate(updates);
  ScrollJankV4Result result = {
      .first_scroll_update = first_scroll_update,
  };

  bool is_janky = false;
  int vsyncs_since_previous_frame = 0;
  if (prev_frame_data_.has_value()) {
    result.running_delivery_cutoff = prev_frame_data_->running_delivery_cutoff;

    // Determine how many VSyncs there have been between the previous and
    // current frame. By default, compare the presentation times. If the current
    // or previous frame's presentation time isn't available, fall back to
    // comparing begin frame times. Sometimes the delta isn't an exact multiple
    // of `vsync_interval`. We add `(vsync_interval / 2)` to round the result
    // properly to the nearest integer.
    base::TimeDelta delta =
        damaging_frame && prev_frame_data_->presentation_ts.has_value()
            ? damaging_frame->presentation_ts -
                  *prev_frame_data_->presentation_ts
            : args.frame_time - prev_frame_data_->begin_frame_ts;
    vsyncs_since_previous_frame = std::clamp(
        base::ClampFloor((delta + (vsync_interval / 2)) / vsync_interval), 1,
        ScrollJankV4Result::kMaxVsyncsSincePreviousFrame);
    result.vsyncs_since_previous_frame = vsyncs_since_previous_frame;

    if (vsyncs_since_previous_frame > 1) {
      // If there was at least one VSync between the previous and current frame,
      // determine whether the current frame should be marked as janky because
      // Chrome should have presented its first inputs (`earliest_event`) in an
      // earlier VSync based on the rules described in
      // https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA.
      std::optional<base::TimeTicks> earliest_input_generation_ts = std::visit(
          absl::Overload{
              [](const ScrollJankV4Result::RealFirstScrollUpdate& real)
                  -> std::optional<base::TimeTicks> {
                return real.actual_input_generation_ts;
              },
              [](const ScrollJankV4Result::SyntheticFirstScrollUpdate&
                     synthetic) {
                return synthetic.extrapolated_input_generation_ts;
              }},
          first_scroll_update);
      JankReasonArray<int> missed_vsyncs_per_reason =
          CalculateMissedVsyncsPerReason(
              vsyncs_since_previous_frame, earliest_input_generation_ts,
              updates, damage, args, treat_as_fast_scroll, result);

      // A frame is janky if ANY of the rules decided that Chrome missed one or
      // more VSyncs.
      is_janky = std::any_of(
          missed_vsyncs_per_reason.begin(), missed_vsyncs_per_reason.end(),
          [](int missed_vsyncs) { return missed_vsyncs > 0; });

      result.missed_vsyncs_per_reason = std::move(missed_vsyncs_per_reason);
    }
  }

  auto presentation = [&]() -> ScrollJankV4Result::Presentation {
    if (damaging_frame) {
      return ScrollJankV4Result::DamagingPresentation{
          .actual_presentation_ts = damaging_frame->presentation_ts};
    }
    if (!prev_frame_data_.has_value() ||
        !prev_frame_data_->presentation_ts.has_value() || is_janky) {
      return ScrollJankV4Result::NonDamagingPresentation{
          .extrapolated_presentation_ts = std::nullopt};
    }
    // If this is a non-damaging frame, we assume that it had the same
    // duration between its begin frame and presentation timestamps as the
    // most recent damaging frame.
    return ScrollJankV4Result::NonDamagingPresentation{
        .extrapolated_presentation_ts =
            *prev_frame_data_->presentation_ts +
            (args.frame_time - prev_frame_data_->begin_frame_ts)};
  }();
  result.presentation = presentation;

  // Finally, update internal state for the next iteration.
  prev_frame_data_ = {
      .has_inertial_input =
          updates.real().has_value() && updates.real()->has_inertial_input,
      .is_most_recent_real_frame_fast_scroll =
          [&]() {
            if (updates.real().has_value()) {
              return treat_as_fast_scroll;
            }
            return prev_frame_data_.has_value() &&
                   prev_frame_data_->is_most_recent_real_frame_fast_scroll;
          }(),
      .last_input_generation_ts = [&]() -> std::optional<base::TimeTicks> {
        if (updates.real().has_value()) {
          return updates.real()->last_input_generation_ts;
        }
        if (!prev_frame_data_.has_value() ||
            !prev_frame_data_->last_input_generation_ts.has_value() ||
            is_janky) {
          return std::nullopt;
        }
        // If this is a synthetic frame, we assume that the synthetic input had
        // the same duration between its generation and original begin frame
        // timestamp as the most recent real frame.
        return *prev_frame_data_->last_input_generation_ts +
               (updates.synthetic()->first_input_begin_frame_ts -
                prev_frame_data_->begin_frame_ts);
      }(),
      .begin_frame_ts = args.frame_time,
      .presentation_ts = std::visit(
          absl::Overload{
              [](const ScrollJankV4Result::DamagingPresentation& damaging)
                  -> std::optional<base::TimeTicks> {
                return damaging.actual_presentation_ts;
              },
              [](const ScrollJankV4Result::NonDamagingPresentation&
                     non_damaging) {
                return non_damaging.extrapolated_presentation_ts;
              }},
          presentation),
      .running_delivery_cutoff = CalculateRunningDeliveryCutoff(
          vsyncs_since_previous_frame, is_janky, updates, damage, args, result),
  };

  return result;
}

// static
bool ScrollJankV4Decider::IsValidFrame(
    const ScrollJankV4FrameStage::ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  if (!args.interval.is_positive()) {
    return false;
  }

  if (const DamagingFrame* damaging_frame =
          std::get_if<DamagingFrame>(&damage)) {
    base::TimeTicks presentation_ts = damaging_frame->presentation_ts;
    if (args.frame_time >= presentation_ts) {
      return false;
    }

    if (updates.real().has_value() &&
        updates.real()->last_input_generation_ts >= presentation_ts) {
      // TODO(crbug.com/40913586): Investigate when these edge cases can be
      // triggered in field and web tests. We have already seen this triggered
      // in field, and some web tests where an event with null(0) timestamp gets
      // coalesced with a "normal" inputs.
      return false;
    }

    if (updates.synthetic().has_value() &&
        updates.synthetic()->first_input_begin_frame_ts >= presentation_ts) {
      return false;
    }
  }

  if (updates.real().has_value() &&
      updates.real()->first_input_generation_ts >
          updates.real()->last_input_generation_ts) {
    return false;
  }

  // Note: We don't use `else if` because `updates` might contain both real and
  // synthetic scroll updates.
  if (updates.synthetic().has_value() &&
      updates.synthetic()->first_input_begin_frame_ts > args.frame_time) {
    return false;
  }

  return true;
}

void ScrollJankV4Decider::OnScrollStarted() {
  Reset();
}

void ScrollJankV4Decider::OnScrollEnded() {
  Reset();
}

// static
bool ScrollJankV4Decider::IsFastScroll(
    const ScrollUpdates::Real& real_updates) {
  static const double kFastScrollContinuityThreshold =
      features::kScrollJankV4MetricFastScrollContinuityThreshold.Get();
  return real_updates.abs_total_raw_delta_pixels >=
         kFastScrollContinuityThreshold;
}

// static
bool ScrollJankV4Decider::IsSufficientlyFastFling(
    const ScrollUpdates::Real& real_updates) {
  static const double kFlingContinuityThreshold =
      features::kScrollJankV4MetricFlingContinuityThreshold.Get();
  return real_updates.max_abs_inertial_raw_delta_pixels >=
         kFlingContinuityThreshold;
}

JankReasonArray<int> ScrollJankV4Decider::CalculateMissedVsyncsPerReason(
    int vsyncs_since_previous_frame,
    std::optional<base::TimeTicks> first_input_generation_ts,
    const ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
    bool treat_as_fast_scroll,
    ScrollJankV4Result& result) const {
  DCHECK_GT(vsyncs_since_previous_frame, 1);
  DCHECK_LE(vsyncs_since_previous_frame,
            ScrollJankV4Result::kMaxVsyncsSincePreviousFrame);

  static const double kStabilityCorrection =
      features::kScrollJankV4MetricStabilityCorrection.Get();
  static const double kDiscountFactor =
      features::kScrollJankV4MetricDiscountFactor.Get();

  JankReasonArray<int> missed_vsyncs_per_reason = {};

  DCHECK(prev_frame_data_.has_value());
  const PreviousFrameData& prev_frame_data = *prev_frame_data_;
  const base::TimeDelta vsync_interval = args.interval;

  // Rule 1: Running consistency.
  // Discount `prev_frame_data.presentation_data->running_delivery_cutoff` based
  // on how many VSyncs there have been since the previous frame (to be a bit
  // more lenient) and subtract stability correction (to be a bit more strict).
  // This is what the current VSync would hypothetically have been judged
  // against if it didn't contain any inputs.
  if (prev_frame_data.running_delivery_cutoff.has_value() &&
      first_input_generation_ts.has_value()) {
    DCHECK(prev_frame_data.presentation_ts.has_value());
    base::TimeTicks presentation_ts = [&]() {
      if (const DamagingFrame* damaging_frame =
              std::get_if<DamagingFrame>(&damage)) {
        return damaging_frame->presentation_ts;
      }
      // If this is a non-damaging frame, assume that it was presented
      // consistently, i.e. it has the same duration between its begin frame and
      // presentation timestamps as the most recent damaging frame, and
      // extrapolate its presentation timestamp. Effectively, this means that
      // this method will evaluate the running consistency rule against this
      // frame's and the previous frame's begin frame timestamps (rather than
      // presentation timestamps).
      return *prev_frame_data_->presentation_ts +
             (args.frame_time - prev_frame_data_->begin_frame_ts);
    }();
    base::TimeDelta adjusted_delivery_cutoff =
        *prev_frame_data.running_delivery_cutoff +
        (vsyncs_since_previous_frame - 1) * kDiscountFactor * vsync_interval -
        kStabilityCorrection * vsync_interval;
    result.adjusted_delivery_cutoff = adjusted_delivery_cutoff;
    base::TimeDelta first_input_to_presentation =
        presentation_ts - *first_input_generation_ts;
    // How much is the current frame's first input delayed compared to Chrome's
    // past performance (`adjusted_delivery_cutoff`)? If positive, input→frame
    // delivery is decelerating. If negative, input→frame delivery is
    // accelerating. Note that we divide by `(1 - kDiscountFactor)` because we
    // need to reverse the discounting as we consider earlier VSyncs.
    base::TimeDelta first_input_delay_compared_to_past_performance =
        (first_input_to_presentation - adjusted_delivery_cutoff) /
        (1 - kDiscountFactor);
    // Based on Chrome's past performance (`adjusted_delivery_cutoff`), how many
    // VSyncs ago could Chrome have presented the current frame's first input?
    int missed_vsyncs_due_to_deceleration = std::clamp(
        base::ClampFloor(first_input_delay_compared_to_past_performance /
                         vsync_interval),
        0, ScrollJankV4Result::kMaxMissedVsyncs);
    if (missed_vsyncs_due_to_deceleration > 0) {
      missed_vsyncs_per_reason[static_cast<int>(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery)] =
          missed_vsyncs_due_to_deceleration;
    }
  }

  // Rules 2 & 3: Fast scroll and fling continuity.
  bool cur_is_sufficiently_fast_fling =
      updates.real().has_value() && IsSufficientlyFastFling(*updates.real());
  if (cur_is_sufficiently_fast_fling) {
    if (prev_frame_data.has_inertial_input) {
      // Chrome missed one or more VSyncs in the middle of a fling.
      missed_vsyncs_per_reason[static_cast<int>(
          JankReason::kMissedVsyncDuringFling)] =
          vsyncs_since_previous_frame - 1;
    } else if (prev_frame_data.is_most_recent_real_frame_fast_scroll) {
      // Chrome missed one or more VSyncs during the transition from a fast
      // regular scroll to a fling.
      missed_vsyncs_per_reason[static_cast<int>(
          JankReason::kMissedVsyncAtStartOfFling)] =
          vsyncs_since_previous_frame - 1;
    }
  } else if (prev_frame_data.is_most_recent_real_frame_fast_scroll &&
             treat_as_fast_scroll) {
    // Chrome missed one or more VSyncs in the middle of a fast regular scroll.
    missed_vsyncs_per_reason[static_cast<int>(
        JankReason::kMissedVsyncDuringFastScroll)] =
        vsyncs_since_previous_frame - 1;
  }

  return missed_vsyncs_per_reason;
}

std::optional<base::TimeDelta>
ScrollJankV4Decider::CalculateRunningDeliveryCutoff(
    int vsyncs_since_previous_frame,
    bool is_janky,
    const ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
    ScrollJankV4Result& result) const {
  // We should consider Chrome's past performance
  // (`*prev_frame_data_->running_delivery_cutoff`) to update
  // the running delivery cut-off as long as there's data available for the
  // previous frame and the current frame is not janky. If there's no data
  // available for the previous frame (because the current frame is the first
  // real damaging frame in the scroll or the first real damaging frame since
  // the decider marked a synthetic and/or non-damaging frame as jank), we start
  // from scratch. Alternatively, if we've just marked the current frame as
  // janky, forget Chrome's past performance and start from scratch.
  std::optional<base::TimeDelta> discounted_prev_delivery_cutoff =
      [&]() -> std::optional<base::TimeDelta> {
    bool should_consider_prev_frame_cutoff =
        prev_frame_data_.has_value() &&
        prev_frame_data_->running_delivery_cutoff.has_value() && !is_janky;
    if (!should_consider_prev_frame_cutoff) {
      return std::nullopt;
    }
    static const double kDiscountFactor =
        features::kScrollJankV4MetricDiscountFactor.Get();
    return *prev_frame_data_->running_delivery_cutoff +
           vsyncs_since_previous_frame * kDiscountFactor * args.interval;
  }();

  const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);

  if (damaging_frame && updates.real().has_value()) {
    base::TimeDelta cur_delivery_cutoff =
        damaging_frame->presentation_ts -
        updates.real()->last_input_generation_ts;
    result.current_delivery_cutoff = cur_delivery_cutoff;
    return discounted_prev_delivery_cutoff.has_value()
               ? std::min(*discounted_prev_delivery_cutoff, cur_delivery_cutoff)
               : cur_delivery_cutoff;
  }

  // If the frame is non-damaging, we don't know when Chrome would actually have
  // presented it. Similarly, if the frame is synthetic, we don't know what's
  // the latest input generation timestamp that Chrome would have included in
  // the frame. Either way, we don't calculate the current frame's delivery
  // cut-off. Instead, we keep discounting the previous frame's delivery
  // cut-off.
  return discounted_prev_delivery_cutoff;
}

ScrollJankV4Result::FirstScrollUpdate ScrollJankV4Decider::GetFirstScrollUpdate(
    const ScrollUpdates& updates) const {
  std::optional<base::TimeTicks>
      extrapolated_first_synthetic_input_generation_ts =
          [&]() -> std::optional<base::TimeTicks> {
    if (!updates.synthetic().has_value() || !prev_frame_data_.has_value() ||
        !prev_frame_data_->last_input_generation_ts.has_value()) {
      return std::nullopt;
    }
    return *prev_frame_data_->last_input_generation_ts +
           (updates.synthetic()->first_input_begin_frame_ts -
            prev_frame_data_->begin_frame_ts);
  }();
  if (updates.real().has_value() &&
      (!extrapolated_first_synthetic_input_generation_ts.has_value() ||
       updates.real()->first_input_generation_ts <=
           *extrapolated_first_synthetic_input_generation_ts)) {
    return ScrollJankV4Result::RealFirstScrollUpdate{
        .actual_input_generation_ts =
            updates.real()->first_input_generation_ts};
  }
  return ScrollJankV4Result::SyntheticFirstScrollUpdate{
      .extrapolated_input_generation_ts =
          extrapolated_first_synthetic_input_generation_ts};
}

void ScrollJankV4Decider::Reset() {
  prev_frame_data_ = std::nullopt;
}

}  // namespace cc
