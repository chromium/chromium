// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
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

ScrollJankV4HistogramEmitter::ScrollJankV4HistogramEmitter() {
  // Not initializing with 0 because the first frame in first window will be
  // always deemed non-janky which makes the metric slightly biased. Setting
  // it to -1 essentially ignores first frame.
  fixed_window_.presented_frames = -1;
}

ScrollJankV4HistogramEmitter::~ScrollJankV4HistogramEmitter() {
  EmitPerScrollHistogramsAndResetCounters();
}

void ScrollJankV4HistogramEmitter::OnFramePresented(
    const JankReasonArray<int>& missed_vsyncs_per_reason) {
  UpdateCountersForPresentedFrame(missed_vsyncs_per_reason);
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
  per_scroll_ = JankDataPerScroll();
}

void ScrollJankV4HistogramEmitter::OnScrollEnded() {
  if (base::FeatureList::IsEnabled(
          features::kEmitPerScrollJankV4MetricAtEndOfScroll)) {
    EmitPerScrollHistogramsAndResetCounters();
  }
}

void ScrollJankV4HistogramEmitter::UpdateCountersForPresentedFrame(
    const JankReasonArray<int>& missed_vsyncs_per_reason) {
  DCHECK_LT(fixed_window_.presented_frames, kHistogramEmitFrequency);

  if (!per_scroll_.has_value()) {
    per_scroll_ = JankDataPerScroll();
  }

  int missed_vsyncs = 0;

  // Update per-reason counters.
  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    int missed_vsyncs_for_reason = missed_vsyncs_per_reason[i];
    if (missed_vsyncs_for_reason == 0) {
      continue;
    }
    missed_vsyncs = std::max(missed_vsyncs, missed_vsyncs_for_reason);
    ++fixed_window_.delayed_frames_per_reason[i];
  }

  bool is_janky = missed_vsyncs > 0;
  if (is_janky) {
    // Update total counters. The scroll jank v4 metric decided that **1 frame**
    // was delayed (hence the `++`) because Chrome missed **`missed_vsyncs`
    // VSyncs** (hence the `+=`).
    ++fixed_window_.delayed_frames;
    ++per_scroll_->delayed_frames;
    fixed_window_.missed_vsyncs += missed_vsyncs;
    fixed_window_.max_consecutive_missed_vsyncs =
        std::max(fixed_window_.max_consecutive_missed_vsyncs, missed_vsyncs);
  }

  // Update counters of presented frames.
  ++fixed_window_.presented_frames;
  ++per_scroll_->presented_frames;
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

  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    JankReason reason = static_cast<JankReason>(i);
    int delayed_frames_for_reason = fixed_window_.delayed_frames_per_reason[i];
    DCHECK_LE(delayed_frames_for_reason, fixed_window_.delayed_frames);
    base::UmaHistogramPercentage(
        GetDelayedFramesPercentageFixedWindow4HistogramName(reason),
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

}  // namespace cc
