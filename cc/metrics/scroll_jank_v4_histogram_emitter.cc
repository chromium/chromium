// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <algorithm>
#include <string>
#include <variant>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/metrics/histogram_macros.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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

static_assert(static_cast<int64_t>(
                  ScrollJankV4HistogramEmitter::kHistogramEmitFrequency + 1) *
                  ScrollJankV4Result::kMaxMissedVsyncs,
              "ScrollJankV4HistogramEmitter::JankDataFixedWindow::"
              "missed_vsyncs might overflow");

}  // namespace

// static
ScrollJankV4HistogramEmitter::SingleFrameData
ScrollJankV4HistogramEmitter::SingleFrameData::From(
    const JankReasonArray<int>& missed_vsyncs_per_reason) {
  SingleFrameData frame_data;

  int missed_vsyncs_for_any_reason = 0;

  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    int missed_vsyncs_for_reason = missed_vsyncs_per_reason[i];
    DCHECK_GE(missed_vsyncs_for_reason, 0);
    DCHECK_LE(missed_vsyncs_for_reason, ScrollJankV4Result::kMaxMissedVsyncs);
    if (missed_vsyncs_for_reason == 0) {
      continue;
    }
    missed_vsyncs_for_any_reason =
        std::max(missed_vsyncs_for_any_reason, missed_vsyncs_for_reason);
    frame_data.jank_reasons.Put(static_cast<JankReason>(i));
  }

  frame_data.missed_vsyncs = missed_vsyncs_for_any_reason;
  frame_data.max_consecutive_missed_vsyncs = missed_vsyncs_for_any_reason;

  return frame_data;
}

void ScrollJankV4HistogramEmitter::SingleFrameData::MergeWith(
    const SingleFrameData& other) {
  jank_reasons.PutAll(other.jank_reasons);
  missed_vsyncs += other.missed_vsyncs;
  max_consecutive_missed_vsyncs = std::max(max_consecutive_missed_vsyncs,
                                           other.max_consecutive_missed_vsyncs);
}

bool ScrollJankV4HistogramEmitter::SingleFrameData::HasJankReasons() const {
  return !jank_reasons.empty();
}

ScrollJankV4HistogramEmitter::ScrollJankV4HistogramEmitter()
    : inner_emitter_(CreateInnerEmitter()) {}

ScrollJankV4HistogramEmitter::~ScrollJankV4HistogramEmitter() {
  // In case ScrollJankV4HistogramEmitter wasn't informed about the end of the
  // last scroll, emit histograms for the last scroll now.
  FinishScroll();
}

void ScrollJankV4HistogramEmitter::OnFrameWithScrollUpdates(
    const JankReasonArray<int>& missed_vsyncs_per_reason,
    bool is_damaging) {
  SingleFrameData frame_data = SingleFrameData::From(missed_vsyncs_per_reason);
  std::visit(
      [&](auto& inner_emitter) {
        inner_emitter.AddFrame(frame_data, is_damaging);
      },
      inner_emitter_);
}

void ScrollJankV4HistogramEmitter::OnScrollStarted() {
  // In case ScrollJankV4HistogramEmitter wasn't informed about the end of the
  // previous scroll, emit histograms for the previous scroll now.
  FinishScroll();
}

void ScrollJankV4HistogramEmitter::OnScrollEnded() {
  FinishScroll();
}

void ScrollJankV4HistogramEmitter::JankDataFixedWindow::AddFrame(
    const SingleFrameData& frame_data) {
  presented_frames++;

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
    delayed_frames_per_reason[static_cast<int>(reason)]++;
  }

  // Update total counters. The scroll jank v4 metric decided that **1 frame**
  // was delayed (hence the `++`) because Chrome missed **`missed_vsyncs`
  // VSyncs** (hence the `+=`).
  ++delayed_frames;
  missed_vsyncs += frame_data.missed_vsyncs;
  max_consecutive_missed_vsyncs = std::max(
      max_consecutive_missed_vsyncs, frame_data.max_consecutive_missed_vsyncs);
}

void ScrollJankV4HistogramEmitter::JankDataFixedWindow::MergeWith(
    const JankDataFixedWindow& other) {
  if (other.presented_frames == 0) {
    DCHECK_EQ(other.delayed_frames, 0);
    DCHECK_EQ(other.missed_vsyncs, 0);
    DCHECK_EQ(other.max_consecutive_missed_vsyncs, 0);
    return;
  }
  presented_frames += other.presented_frames;
  delayed_frames += other.delayed_frames;
  missed_vsyncs += other.missed_vsyncs;
  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    delayed_frames_per_reason[i] += other.delayed_frames_per_reason[i];
  }
  max_consecutive_missed_vsyncs = std::max(max_consecutive_missed_vsyncs,
                                           other.max_consecutive_missed_vsyncs);
}

void ScrollJankV4HistogramEmitter::JankDataPerScroll::AddFrame(
    const SingleFrameData& frame_data) {
  presented_frames++;
  if (frame_data.HasJankReasons()) {
    delayed_frames++;
  }
}

void ScrollJankV4HistogramEmitter::JankDataPerScroll::MergeWith(
    const JankDataPerScroll& other) {
  if (other.presented_frames == 0) {
    DCHECK_EQ(other.delayed_frames, 0);
    return;
  }
  presented_frames += other.presented_frames;
  delayed_frames += other.delayed_frames;
}

void ScrollJankV4HistogramEmitter::EmitForAllScrolls::AddFrame(
    const SingleFrameData& frame_data,
    bool is_damaging) {
  DCHECK_LT(fixed_window_.presented_frames, kHistogramEmitFrequency);
  fixed_window_.AddFrame(frame_data);
  per_scroll_.AddFrame(frame_data);
  MaybeEmitPerWindowHistogramsAndResetCounters();
  DCHECK_LT(fixed_window_.presented_frames, kHistogramEmitFrequency);
}

void ScrollJankV4HistogramEmitter::EmitForAllScrolls::FinishScroll() {
  MaybeEmitPerScrollHistogramsAndResetCounters();
}

void ScrollJankV4HistogramEmitter::EmitForAllScrolls::
    MaybeEmitPerWindowHistogramsAndResetCounters() {
  DCHECK_LE(fixed_window_.presented_frames, kHistogramEmitFrequency);
  DCHECK_LE(fixed_window_.delayed_frames, fixed_window_.presented_frames);
  DCHECK_GE(fixed_window_.missed_vsyncs, fixed_window_.delayed_frames);
  DCHECK_LE(fixed_window_.max_consecutive_missed_vsyncs,
            fixed_window_.missed_vsyncs);

  if (fixed_window_.presented_frames < kHistogramEmitFrequency) {
    return;
  }

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

void ScrollJankV4HistogramEmitter::EmitForAllScrolls::
    MaybeEmitPerScrollHistogramsAndResetCounters() {
  DCHECK_GE(per_scroll_.presented_frames, per_scroll_.delayed_frames);

  if (per_scroll_.presented_frames == 0) {
    return;
  }

  UMA_HISTOGRAM_PERCENTAGE(
      kDelayedFramesPerScrollHistogram,
      (100 * per_scroll_.delayed_frames) / per_scroll_.presented_frames);

  per_scroll_ = JankDataPerScroll();
}

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::EmitForDamagingScrolls() =
    default;

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::EmitForDamagingScrolls(
    const EmitForDamagingScrolls&) = default;

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::
    ~EmitForDamagingScrolls() = default;

void ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::AddFrame(
    const SingleFrameData& frame_data,
    bool is_damaging) {
  if (std::holds_alternative<NoDamagingFrameEncounteredYet>(state_)) {
    if (!is_damaging) {
      StashPendingFrame(frame_data);
      return;
    }
    FlushPendingFrames();
  }

  DCHECK(std::holds_alternative<DamagingFrameAlreadyEncountered>(state_));
  wrapped_emitter_.AddFrame(frame_data, is_damaging);
}

void ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::FinishScroll() {
  wrapped_emitter_.FinishScroll();
  if (const auto* state = std::get_if<NoDamagingFrameEncounteredYet>(&state_);
      state != nullptr && state->pending_per_scroll.delayed_frames > 0) {
    TRACE_EVENT("input",
                "ScrollJankV4HistogramEmitter: Metric missed jank in a "
                "non-damaging scroll");
  }
  state_ = NoDamagingFrameEncounteredYet();
}

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::
    NoDamagingFrameEncounteredYet::NoDamagingFrameEncounteredYet() = default;

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::
    NoDamagingFrameEncounteredYet::NoDamagingFrameEncounteredYet(
        const NoDamagingFrameEncounteredYet&) = default;

ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::
    NoDamagingFrameEncounteredYet::~NoDamagingFrameEncounteredYet() = default;

void ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::StashPendingFrame(
    const SingleFrameData& frame_data) {
  DCHECK(std::holds_alternative<NoDamagingFrameEncounteredYet>(state_));
  auto& state = std::get<NoDamagingFrameEncounteredYet>(state_);

  // Determine whether we need to append a new item to
  // `state.pending_fixed_windows`.
  bool need_new_window = [&] {
    switch (state.pending_fixed_windows.size()) {
      case 0:
        return true;
      case 1:
        // The first item in `state.pending_fixed_windows` will likely be merged
        // into `wrapped_emitter_.fixed_window_`, so we need to combine their
        // sizes.
        return state.pending_fixed_windows.front().presented_frames +
                   wrapped_emitter_.fixed_window_.presented_frames ==
               kHistogramEmitFrequency;
      default:
        return state.pending_fixed_windows.back().presented_frames ==
               kHistogramEmitFrequency;
    }
  }();
  if (need_new_window) {
    // Don't let `state.pending_fixed_windows` grow unbounded.
    if (state.pending_fixed_windows.size() >=
        NoDamagingFrameEncounteredYet::kPendingFixedWindowsMaxSize) {
      TRACE_EVENT("input",
                  "ScrollJankV4HistogramEmitter: Ignoring a non-damaging frame "
                  "because there are too many non-damaging frames at the "
                  "beginning of a scroll");
      return;
    }
    state.pending_fixed_windows.push_back(JankDataFixedWindow());
  }

  // Add the frame to pending data, waiting to see if there's a later damaging
  // frame in the current scroll.
  state.pending_fixed_windows.back().AddFrame(frame_data);
  state.pending_per_scroll.AddFrame(frame_data);
}

void ScrollJankV4HistogramEmitter::EmitForDamagingScrolls::
    FlushPendingFrames() {
  DCHECK(std::holds_alternative<NoDamagingFrameEncounteredYet>(state_));

  {
    const auto& state = std::get<NoDamagingFrameEncounteredYet>(state_);
    for (const auto& pending_fixed_window : state.pending_fixed_windows) {
      wrapped_emitter_.fixed_window_.MergeWith(pending_fixed_window);
      wrapped_emitter_.MaybeEmitPerWindowHistogramsAndResetCounters();
    }
    wrapped_emitter_.per_scroll_.MergeWith(state.pending_per_scroll);
  }

  state_ = DamagingFrameAlreadyEncountered();
}

// static
ScrollJankV4HistogramEmitter::InnerEmitter
ScrollJankV4HistogramEmitter::CreateInnerEmitter() {
  // If the scroll jank v4 metric doesn't handle non-damaging scroll updates at
  // all, then all frames are considered damaging, so emit for all frames.
  if (!base::FeatureList::IsEnabled(
          features::kHandleNonDamagingInputsInScrollJankV4Metric)) {
    return EmitForAllScrolls();
  }

  const std::string histogram_emission_policy =
      features::kHistogramEmissionPolicy.Get();
  if (histogram_emission_policy == features::kEmitForAllScrolls) {
    return EmitForAllScrolls();
  } else if (histogram_emission_policy == features::kEmitForDamagingScrolls) {
    return EmitForDamagingScrolls();
  }

  // If `features::kHistogramEmissionPolicy` is invalid, default to emitting for
  // damaging scrolls.
  return EmitForDamagingScrolls();
}

void ScrollJankV4HistogramEmitter::FinishScroll() {
  std::visit([](auto& inner_emitter) { inner_emitter.FinishScroll(); },
             inner_emitter_);
}

}  // namespace cc
