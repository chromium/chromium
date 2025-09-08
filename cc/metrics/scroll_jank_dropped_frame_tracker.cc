// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

namespace {

// Histogram min, max and no. of buckets.
constexpr int kVsyncCountsMin = 1;
constexpr int kVsyncCountsMax = 50;
constexpr int kVsyncCountsBuckets = 25;

constexpr const char* GetDelayedFramesPercentageFixedWindow4HistogramName(
    JankReason reason) {
#define CASE(reason)       \
  case JankReason::reason: \
    return ScrollJankDroppedFrameTracker::reason##V4Histogram;
  switch (reason) {
    CASE(kMissedVsyncDueToDeceleratingInputFrameDelivery);
    CASE(kMissedVsyncDuringFastScroll);
    CASE(kMissedVsyncAtStartOfFling);
    CASE(kMissedVsyncDuringFling);
    default:
      NOTREACHED();
  }
#undef CASE
}

}  // namespace

ScrollJankDroppedFrameTracker::ScrollJankDroppedFrameTracker() {
  // Not initializing with 0 because the first frame in first window will be
  // always deemed non-janky which makes the metric slightly biased. Setting
  // it to -1 essentially ignores first frame.
  fixed_window_.num_presented_frames = -1;
  fixed_window_v4_.presented_frames = -1;
}

ScrollJankDroppedFrameTracker::~ScrollJankDroppedFrameTracker() {
  if (per_scroll_.has_value()) {
    // Per scroll metrics for a given scroll are emitted at the start of next
    // scroll. Emittimg from here makes sure we don't loose the data for last
    // scroll.
    EmitPerScrollHistogramsAndResetCounters();
    EmitPerScrollV4HistogramsAndResetCounters();
  }
}

void ScrollJankDroppedFrameTracker::EmitPerScrollHistogramsAndResetCounters() {
  if (!per_scroll_.has_value()) {
    return;
  }

  // There should be at least one presented frame given the method is only
  // called after we have a successful presentation.
  if (per_scroll_->num_presented_frames == 0) {
    // TODO(crbug.com/40067426): Debug cases where we can have 0 presented
    // frames.
    TRACE_EVENT_INSTANT("input", "NoPresentedFramesInScroll");
    per_scroll_ = std::nullopt;
    return;
  }
  int delayed_frames_percentage =
      (100 * per_scroll_->missed_frames) / per_scroll_->num_presented_frames;
  UMA_HISTOGRAM_PERCENTAGE(kDelayedFramesPerScrollHistogram,
                           delayed_frames_percentage);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxPerScrollHistogram,
                              per_scroll_->max_missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumPerScrollHistogram,
                              per_scroll_->missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);

  per_scroll_ = std::nullopt;
}

void ScrollJankDroppedFrameTracker::
    EmitPerScrollV4HistogramsAndResetCounters() {
  if (!per_scroll_v4_.has_value()) {
    return;
  }

  DCHECK_GE(per_scroll_v4_->presented_frames, per_scroll_v4_->delayed_frames);

  // There should be at least one presented frame given the method is only
  // called after we have a successful presentation.
  if (per_scroll_v4_->presented_frames > 0) {
    UMA_HISTOGRAM_PERCENTAGE(kDelayedFramesPerScrollV4Histogram,
                             (100 * per_scroll_v4_->delayed_frames) /
                                 per_scroll_v4_->presented_frames);
  }

  per_scroll_v4_ = std::nullopt;
}

void ScrollJankDroppedFrameTracker::EmitPerWindowHistogramsAndResetCounters() {
  DCHECK_EQ(fixed_window_.num_presented_frames, kHistogramEmitFrequency);

  UMA_HISTOGRAM_PERCENTAGE(
      kDelayedFramesWindowHistogram,
      (100 * fixed_window_.missed_frames) / kHistogramEmitFrequency);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumInWindowHistogram,
                              fixed_window_.missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxInWindowHistogram,
                              fixed_window_.max_missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);

  fixed_window_.missed_frames = 0;
  fixed_window_.missed_vsyncs = 0;
  fixed_window_.max_missed_vsyncs = 0;
  // We don't need to reset it to -1 because after the first window we always
  // have a valid previous frame data to compare the first frame of window.
  fixed_window_.num_presented_frames = 0;
}

void ScrollJankDroppedFrameTracker::
    EmitPerWindowV4HistogramsAndResetCounters() {
  DCHECK_EQ(fixed_window_v4_.presented_frames, kHistogramEmitFrequency);
  DCHECK_LE(fixed_window_v4_.delayed_frames, fixed_window_v4_.presented_frames);
  DCHECK_GE(fixed_window_v4_.missed_vsyncs, fixed_window_v4_.delayed_frames);
  DCHECK_LE(fixed_window_v4_.max_consecutive_missed_vsyncs,
            fixed_window_v4_.missed_vsyncs);

  UMA_HISTOGRAM_PERCENTAGE(
      kDelayedFramesWindowV4Histogram,
      (100 * fixed_window_v4_.delayed_frames) / kHistogramEmitFrequency);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumInWindowV4Histogram,
                              fixed_window_v4_.missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxInWindowV4Histogram,
                              fixed_window_v4_.max_consecutive_missed_vsyncs,
                              kVsyncCountsMin, kVsyncCountsMax,
                              kVsyncCountsBuckets);

  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    JankReason reason = static_cast<JankReason>(i);
    int delayed_frames_for_reason =
        fixed_window_v4_.delayed_frames_per_reason[i];
    DCHECK_LE(delayed_frames_for_reason, fixed_window_v4_.delayed_frames);
    base::UmaHistogramPercentage(
        GetDelayedFramesPercentageFixedWindow4HistogramName(reason),
        (100 * delayed_frames_for_reason) / kHistogramEmitFrequency);
  }

  // We don't need to reset these to -1 because after the first window we always
  // have a valid previous frame data to compare the first frame of window.
  fixed_window_v4_ = JankDataFixedWindowV4();
}

void ScrollJankDroppedFrameTracker::ReportLatestPresentationData(
    ScrollUpdateEventMetrics& earliest_event,
    ScrollUpdateEventMetrics& latest_event,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval,
    bool has_inertial_input,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels) {
  base::TimeTicks first_input_generation_ts =
      latest_event.GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);
  base::TimeTicks first_input_generation_v4_ts =
      earliest_event.GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);
  CHECK_LE(first_input_generation_v4_ts, first_input_generation_ts);
  CHECK(has_inertial_input || max_abs_inertial_raw_delta_pixels == 0);
  if ((last_input_generation_ts < first_input_generation_ts) ||
      (presentation_ts <= last_input_generation_ts)) {
    // TODO(crbug.com/40913586): Investigate when these edge cases can be
    // triggered in field and web tests. We have already seen this triggered in
    // field, and some web tests where an event with null(0) timestamp gets
    // coalesced with a "normal" input.
    return;
  }
  // TODO(b/276722271) : Analyze and reduce these cases of out of order
  // frame termination.
  if (presentation_ts <= prev_presentation_ts_) {
    TRACE_EVENT_INSTANT("input", "OutOfOrderTerminatedFrame");
    return;
  }

  // `per_scroll_` is initialized in OnScrollStarted when we see
  // FIRST_GESTURE_SCROLL_UPDATE event. But in some rare scenarios we don't see
  // the FIRST_GESTURE_SCROLL_UPDATE events on scroll start.
  if (!per_scroll_.has_value()) {
    per_scroll_ = JankData();
  }

  // The presentation delta is usually 16.6ms for 60 Hz devices,
  // but sometimes random errors result in a delta of up to 20ms
  // as observed in traces. This adds an error margin of 1/2 a
  // vsync before considering the Vsync missed.
  bool missed_frame = (presentation_ts - prev_presentation_ts_) >
                      (vsync_interval + vsync_interval / 2);
  bool input_available =
      (first_input_generation_ts - prev_last_input_generation_ts_) <
      (vsync_interval + vsync_interval / 2);

  // Sometimes the vsync interval is not accurate and is slightly more
  // than the actual signal arrival time, adding (vsync_interval / 2)
  // here insures the result is always ceiled.
  int curr_frame_total_vsyncs =
      (presentation_ts - prev_presentation_ts_ + (vsync_interval / 2)) /
      vsync_interval;
  int curr_frame_missed_vsyncs = curr_frame_total_vsyncs - 1;

  if (missed_frame && input_available) {
    ++fixed_window_.missed_frames;
    ++per_scroll_->missed_frames;
    UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsPerFrameHistogram,
                                curr_frame_missed_vsyncs, kVsyncCountsMin,
                                kVsyncCountsMax, kVsyncCountsBuckets);
    fixed_window_.missed_vsyncs += curr_frame_missed_vsyncs;
    per_scroll_->missed_vsyncs += curr_frame_missed_vsyncs;

    if (scroll_jank_ukm_reporter_) {
      scroll_jank_ukm_reporter_->IncrementDelayedFrameCount();
      scroll_jank_ukm_reporter_->AddMissedVsyncs(curr_frame_missed_vsyncs);
    }

    if (curr_frame_missed_vsyncs > per_scroll_->max_missed_vsyncs) {
      per_scroll_->max_missed_vsyncs = curr_frame_missed_vsyncs;
      if (scroll_jank_ukm_reporter_) {
        scroll_jank_ukm_reporter_->set_max_missed_vsyncs(
            curr_frame_missed_vsyncs);
      }
    }
    if (curr_frame_missed_vsyncs > fixed_window_.max_missed_vsyncs) {
      fixed_window_.max_missed_vsyncs = curr_frame_missed_vsyncs;
    }

    TRACE_EVENT_INSTANT(
        "input,input.scrolling", "MissedFrame", "per_scroll_->missed_frames",
        per_scroll_->missed_frames, "per_scroll_->missed_vsyncs",
        per_scroll_->missed_vsyncs, "vsync_interval", vsync_interval);
    latest_event.set_is_janky_scrolled_frame(true);
  } else {
    latest_event.set_is_janky_scrolled_frame(false);
    UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsPerFrameHistogram, 0,
                                kVsyncCountsMin, kVsyncCountsMax,
                                kVsyncCountsBuckets);
  }

  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->AddVsyncs(
        input_available ? curr_frame_total_vsyncs : 1);
  }

  ++fixed_window_.num_presented_frames;
  ++per_scroll_->num_presented_frames;
  if (scroll_jank_ukm_reporter_) {
    scroll_jank_ukm_reporter_->IncrementFrameCount();
  }

  if (fixed_window_.num_presented_frames == kHistogramEmitFrequency) {
    EmitPerWindowHistogramsAndResetCounters();
  }
  DCHECK_LT(fixed_window_.num_presented_frames, kHistogramEmitFrequency);

  ReportLatestPresentationDataV4(
      earliest_event, first_input_generation_v4_ts, last_input_generation_ts,
      presentation_ts, vsync_interval, has_inertial_input,
      abs_total_raw_delta_pixels, max_abs_inertial_raw_delta_pixels);

  prev_presentation_ts_ = presentation_ts;
  prev_last_input_generation_ts_ = last_input_generation_ts;
}

void ScrollJankDroppedFrameTracker::ReportLatestPresentationDataV4(
    ScrollUpdateEventMetrics& earliest_event,
    base::TimeTicks first_input_generation_v4_ts,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval,
    bool has_inertial_input,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels) {
  static const bool scroll_jank_v4_metric_enabled =
      base::FeatureList::IsEnabled(features::kScrollJankV4Metric);
  if (!scroll_jank_v4_metric_enabled) {
    return;
  }

  if (!per_scroll_v4_.has_value()) {
    per_scroll_v4_ = JankDataPerScrollV4();
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
        presentation_ts - prev_presentation_ts_;
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
          CalculateMissedVsyncsPerReasonV4(
              vsyncs_since_previous_frame, first_input_generation_v4_ts,
              presentation_ts, vsync_interval, abs_total_raw_delta_pixels,
              max_abs_inertial_raw_delta_pixels, result);

      // A frame is janky if ANY of the rules decided that Chrome missed one or
      // more VSyncs.
      is_janky = std::any_of(
          missed_vsyncs_per_reason.begin(), missed_vsyncs_per_reason.end(),
          [](int missed_vsyncs) { return missed_vsyncs > 0; });
      if (is_janky) {
        UpdateDelayedFrameAndMissedVsyncCountersV4(missed_vsyncs_per_reason);
      }

      result.missed_vsyncs_per_reason = std::move(missed_vsyncs_per_reason);
    }
  }

  // Update counters of presented frames.
  ++fixed_window_v4_.presented_frames;
  ++per_scroll_v4_->presented_frames;

  // Emit per-window histograms if we've reached the end of the current window.
  if (fixed_window_v4_.presented_frames == kHistogramEmitFrequency) {
    EmitPerWindowV4HistogramsAndResetCounters();
  }
  DCHECK_LT(fixed_window_v4_.presented_frames, kHistogramEmitFrequency);

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
      .has_inertial_input = has_inertial_input,
      .abs_total_raw_delta_pixels = abs_total_raw_delta_pixels,
      .running_delivery_cutoff = new_running_delivery_cutoff,
  };

  DCHECK(!earliest_event.scroll_jank_v4());
  earliest_event.set_scroll_jank_v4(std::move(result));
}

JankReasonArray<int>
ScrollJankDroppedFrameTracker::CalculateMissedVsyncsPerReasonV4(
    int vsyncs_since_previous_frame,
    base::TimeTicks first_input_generation_v4_ts,
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
  const PreviousFrameDataV4& prev_frame_data = *prev_frame_data_;

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
      presentation_ts - first_input_generation_v4_ts;
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

void ScrollJankDroppedFrameTracker::UpdateDelayedFrameAndMissedVsyncCountersV4(
    const JankReasonArray<int> missed_vsyncs_per_reason) {
  int missed_vsyncs = 0;

  // Update per-reason counters.
  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    int missed_vsyncs_for_reason = missed_vsyncs_per_reason[i];
    if (missed_vsyncs_for_reason == 0) {
      continue;
    }
    missed_vsyncs = std::max(missed_vsyncs, missed_vsyncs_for_reason);
    ++fixed_window_v4_.delayed_frames_per_reason[i];
  }

  bool is_janky = missed_vsyncs > 0;
  if (is_janky) {
    // Update total counters. The scroll jank v4 metric decided that **1 frame**
    // was delayed (hence the `++`) because Chrome missed **`missed_vsyncs`
    // VSyncs** (hence the `+=`).
    ++fixed_window_v4_.delayed_frames;
    ++per_scroll_v4_->delayed_frames;
    fixed_window_v4_.missed_vsyncs += missed_vsyncs;
    fixed_window_v4_.max_consecutive_missed_vsyncs =
        std::max(fixed_window_v4_.max_consecutive_missed_vsyncs, missed_vsyncs);
  }
}

void ScrollJankDroppedFrameTracker::OnScrollStarted() {
  // In case ScrollJankDroppedFrameTracker wasn't informed about the end of the
  // previous scroll, emit histograms for the previous scroll now.
  EmitPerScrollHistogramsAndResetCounters();
  EmitPerScrollV4HistogramsAndResetCounters();
  per_scroll_ = JankData();
  per_scroll_v4_ = JankDataPerScrollV4();
  prev_frame_data_ = std::nullopt;
}

void ScrollJankDroppedFrameTracker::OnScrollEnded() {
  if (base::FeatureList::IsEnabled(
          features::kEmitPerScrollJankV1MetricAtEndOfScroll)) {
    EmitPerScrollHistogramsAndResetCounters();
  }
  if (base::FeatureList::IsEnabled(
          features::kEmitPerScrollJankV4MetricAtEndOfScroll)) {
    EmitPerScrollV4HistogramsAndResetCounters();
  }
}

}  // namespace cc
