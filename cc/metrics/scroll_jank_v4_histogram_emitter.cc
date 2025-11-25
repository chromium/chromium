// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/histogram_macros.h"
#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

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
    return ScrollJankV4HistogramEmitter::reason##Histogram;
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

bool ScrollJankV4HistogramEmitter::SingleFrameData::HasJankReasons() const {
  return !jank_reasons.empty();
}

void ScrollJankV4HistogramEmitter::SingleFrameData::UpdateWith(
    const JankReasonArray<int>& missed_vsyncs_per_reason) {
  int missed_vsyncs_for_any_reason = 0;

  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    int missed_vsyncs_for_reason = missed_vsyncs_per_reason[i];
    if (missed_vsyncs_for_reason == 0) {
      continue;
    }
    missed_vsyncs_for_any_reason =
        std::max(missed_vsyncs_for_any_reason, missed_vsyncs_for_reason);
    jank_reasons.Put(static_cast<JankReason>(i));
  }

  missed_vsyncs += missed_vsyncs_for_any_reason;
  max_consecutive_missed_vsyncs =
      std::max(max_consecutive_missed_vsyncs, missed_vsyncs_for_any_reason);
}

ScrollJankV4HistogramEmitter::ScrollJankV4HistogramEmitter() {
  // Not initializing with 0 because the first frame in first window will be
  // always deemed non-janky which makes the metric slightly biased. Setting
  // it to -1 essentially ignores first frame.
  fixed_window_.presented_frames = -1;
}

ScrollJankV4HistogramEmitter::~ScrollJankV4HistogramEmitter() {
  EmitPerScrollHistogramsAndResetCounters();
}

void ScrollJankV4HistogramEmitter::OnFrameWithScrollUpdates(
    const JankReasonArray<int>& missed_vsyncs_per_reason,
    bool counts_towards_histogram_frame_count) {
  DCHECK_LT(fixed_window_.presented_frames, kHistogramEmitFrequency);

  if (!counts_towards_histogram_frame_count) {
    if (!accumulated_data_from_non_damaging_frames_.has_value()) {
      accumulated_data_from_non_damaging_frames_ = SingleFrameData();
    }
    accumulated_data_from_non_damaging_frames_->UpdateWith(
        missed_vsyncs_per_reason);
    return;
  }

  SingleFrameData frame_data = {};

  // If we've accumulated any jank data from non-damaging frames, consume it
  // together with the current frame.
  if (accumulated_data_from_non_damaging_frames_.has_value()) {
    frame_data = *accumulated_data_from_non_damaging_frames_;
    accumulated_data_from_non_damaging_frames_ = std::nullopt;
  }

  frame_data.UpdateWith(missed_vsyncs_per_reason);
  UpdateCountersForFrame(frame_data);

  // Emit per-window histograms if we've reached the end of the current window.
  if (fixed_window_.presented_frames == kHistogramEmitFrequency) {
    EmitPerWindowHistogramsAndResetCounters();
  }
  DCHECK_LT(fixed_window_.presented_frames, kHistogramEmitFrequency);
}

void ScrollJankV4HistogramEmitter::OnScrollStarted() {
  // In case ScrollJankDroppedFrameTracker wasn't informed about the end of the
  // previous scroll, emit histograms for the previous scroll now.
  EmitPerScrollHistogramsAndResetCounters();
  // Don't carry jank data from non-damaging frames across scrolls.
  ResetAccumulatedDataFromNonDamagingFrames();
  per_scroll_ = JankDataPerScroll();
}

void ScrollJankV4HistogramEmitter::OnScrollEnded() {
  EmitPerScrollHistogramsAndResetCounters();
  // Don't carry jank data from non-damaging frames across scrolls.
  ResetAccumulatedDataFromNonDamagingFrames();
}

void ScrollJankV4HistogramEmitter::UpdateCountersForFrame(
    const SingleFrameData& frame_data) {
  if (!per_scroll_.has_value()) {
    per_scroll_ = JankDataPerScroll();
  }

  ++fixed_window_.presented_frames;
  ++per_scroll_->presented_frames;

  if (!frame_data.HasJankReasons()) {
    // No jank reasons => no need to update any delayed/missed counters.
    DCHECK_EQ(frame_data.missed_vsyncs, 0);
    DCHECK_EQ(frame_data.max_consecutive_missed_vsyncs, 0);
    return;
  }

  // At least one jank reason => there must be at least one missed VSync.
  DCHECK_GT(frame_data.missed_vsyncs, 0);
  DCHECK_GE(frame_data.missed_vsyncs, frame_data.max_consecutive_missed_vsyncs);

  // Update per-reason counters.
  for (JankReason reason : frame_data.jank_reasons) {
    fixed_window_.delayed_frames_per_reason[static_cast<int>(reason)]++;
  }

  // Update total counters. The scroll jank v4 metric decided that **1 frame**
  // was delayed (hence the `++`) because Chrome missed **`missed_vsyncs`
  // VSyncs** (hence the `+=`).
  ++fixed_window_.delayed_frames;
  ++per_scroll_->delayed_frames;
  fixed_window_.missed_vsyncs += frame_data.missed_vsyncs;
  fixed_window_.max_consecutive_missed_vsyncs =
      std::max(fixed_window_.max_consecutive_missed_vsyncs,
               frame_data.max_consecutive_missed_vsyncs);
}

void ScrollJankV4HistogramEmitter::EmitPerWindowHistogramsAndResetCounters() {
  DCHECK_EQ(fixed_window_.presented_frames, kHistogramEmitFrequency);
  DCHECK_LE(fixed_window_.delayed_frames, fixed_window_.presented_frames);
  DCHECK_GE(fixed_window_.missed_vsyncs, fixed_window_.delayed_frames);
  DCHECK_LE(fixed_window_.max_consecutive_missed_vsyncs,
            fixed_window_.missed_vsyncs);

  UMA_HISTOGRAM_PERCENTAGE(
      kDelayedFramesWindowHistogram,
      (100 * fixed_window_.delayed_frames) / kHistogramEmitFrequency);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsSumInWindowHistogram,
                              fixed_window_.missed_vsyncs, kVsyncCountsMin,
                              kVsyncCountsMax, kVsyncCountsBuckets);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMissedVsyncsMaxInWindowHistogram,
                              fixed_window_.max_consecutive_missed_vsyncs,
                              kVsyncCountsMin, kVsyncCountsMax,
                              kVsyncCountsBuckets);

  constexpr int kMaxJankReasonIndex = static_cast<int>(JankReason::kMaxValue);
  for (int i = 0; i <= kMaxJankReasonIndex; i++) {
    JankReason reason = static_cast<JankReason>(i);
    int delayed_frames_for_reason = fixed_window_.delayed_frames_per_reason[i];
    DCHECK_LE(delayed_frames_for_reason, fixed_window_.delayed_frames);
    STATIC_HISTOGRAM_PERCENTAGE_POINTER_GROUP(
        GetDelayedFramesPercentageFixedWindow4HistogramName(reason), i,
        kMaxJankReasonIndex + 1,
        (100 * delayed_frames_for_reason) / kHistogramEmitFrequency);
  }

  // We don't need to reset these to -1 because after the first window we always
  // have a valid previous frame data to compare the first frame of window.
  fixed_window_ = JankDataFixedWindow();
}

void ScrollJankV4HistogramEmitter::EmitPerScrollHistogramsAndResetCounters() {
  if (!per_scroll_.has_value()) {
    return;
  }

  DCHECK_GE(per_scroll_->presented_frames, per_scroll_->delayed_frames);

  // There should be at least one presented frame given the method is only
  // called after we have a successful presentation.
  if (per_scroll_->presented_frames > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        kDelayedFramesPerScrollHistogram,
        (100 * per_scroll_->delayed_frames) / per_scroll_->presented_frames);
  }

  per_scroll_ = std::nullopt;
}

void ScrollJankV4HistogramEmitter::ResetAccumulatedDataFromNonDamagingFrames() {
  if (!accumulated_data_from_non_damaging_frames_.has_value()) {
    return;
  }
  if (accumulated_data_from_non_damaging_frames_->HasJankReasons()) {
    TRACE_EVENT("input",
                "ScrollJankV4HistogramEmitter: Metric missed jank from "
                "non-damaging frames");
  }
  accumulated_data_from_non_damaging_frames_ = std::nullopt;
}

}  // namespace cc
