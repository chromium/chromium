// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decision_queue.h"

#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "cc/metrics/scroll_jank_v4_decider.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;

}  // namespace

ScrollJankV4DecisionQueue::ResultConsumer::~ResultConsumer() = default;

ScrollJankV4DecisionQueue::ScrollJankV4DecisionQueue(
    std::unique_ptr<ResultConsumer> result_consumer)
    : result_consumer_(std::move(result_consumer)) {}

ScrollJankV4DecisionQueue::~ScrollJankV4DecisionQueue() {
  FlushDeferredSyntheticFrames(
      /* future_real_frame_is_fast_scroll_or_sufficiently_fast_fling= */ false);
}

bool ScrollJankV4DecisionQueue::ProcessFrameWithScrollUpdates(
    ScrollUpdates& updates,
    const ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  // TODO(crbug.com/464210135): Enforce the below invariant about
  // `has_inertial_input` and `max_abs_inertial_raw_delta_pixels` in
  // `ScrollUpdates::Real`.
  CHECK(!updates.real().has_value() || updates.real()->has_inertial_input ||
        updates.real()->max_abs_inertial_raw_delta_pixels == 0);

  if (!AcceptFrameIfValidAndChronological(updates, damage, args)) {
    return false;
  }

  // If the frame contains only synthetic inputs, defer the decision until we
  // receive a frame with at least one real input.
  if (!updates.real().has_value()) {
    // Set `earliest_event` to null to avoid a dangling pointer.
    ScrollUpdates updates_without_earliest_event = ScrollUpdates(
        /* earliest_event= */ nullptr, updates.real(), updates.synthetic());
    deferred_synthetic_frames_.emplace_back(updates_without_earliest_event,
                                            damage, args);
    return true;
  }

  // If the new frame contains at least one real input, we have enough
  // information to decide if any preceding synthetic frames and the new frame
  // itself are janky (in chronological order).
  const ScrollUpdates::Real& real_updates = *updates.real();
  bool future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
      ScrollJankV4Decider::IsFastScroll(real_updates) ||
      ScrollJankV4Decider::IsSufficientlyFastFling(real_updates);
  FlushDeferredSyntheticFrames(
      future_real_frame_is_fast_scroll_or_sufficiently_fast_fling);
  auto result =
      decider_.DecideJankForFrameWithRealScrollUpdates(updates, damage, args);
  result_consumer_->OnFrameResult(result, updates.earliest_event());
  return true;
}

void ScrollJankV4DecisionQueue::OnScrollStarted() {
  // There should be no deferred synthetic frames to flush UNLESS we didn't
  // receive the scroll end event for some reason.
  FlushDeferredSyntheticFrames(
      /* future_real_frame_is_fast_scroll_or_sufficiently_fast_fling= */ false);
  decider_.OnScrollStarted();
  result_consumer_->OnScrollStarted();
}

void ScrollJankV4DecisionQueue::OnScrollEnded() {
  FlushDeferredSyntheticFrames(
      /* future_real_frame_is_fast_scroll_or_sufficiently_fast_fling= */ false);
  decider_.OnScrollEnded();
  result_consumer_->OnScrollEnded();
}

bool ScrollJankV4DecisionQueue::AcceptFrameIfValidAndChronological(
    const ScrollJankV4FrameStage::ScrollUpdates& updates,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  // Check that the frame is valid and that it came after the most recently
  // provided frame.
  if (!ScrollJankV4Decider::IsValidFrame(updates, damage, args)) {
    return false;
  }
  if (args.frame_time <= last_provided_valid_begin_frame_ts_) {
    return false;
  }
  const DamagingFrame* damaging_frame = std::get_if<DamagingFrame>(&damage);
  if (damaging_frame &&
      damaging_frame->presentation_ts <= last_provided_valid_presentation_ts_) {
    return false;
  }

  // Accept the new frame.
  last_provided_valid_begin_frame_ts_ = args.frame_time;
  if (damaging_frame) {
    last_provided_valid_presentation_ts_ = damaging_frame->presentation_ts;
  }
  return true;
}

void ScrollJankV4DecisionQueue::FlushDeferredSyntheticFrames(
    bool future_real_frame_is_fast_scroll_or_sufficiently_fast_fling) {
  for (const auto& [updates, damage, args] : deferred_synthetic_frames_) {
    DCHECK(!updates.real().has_value());
    DCHECK_EQ(updates.earliest_event(), nullptr);
    auto result = decider_.DecideJankForFrameWithSyntheticScrollUpdatesOnly(
        updates, damage, args,
        future_real_frame_is_fast_scroll_or_sufficiently_fast_fling);
    result_consumer_->OnFrameResult(result, /* earliest_event= */ nullptr);
  }
  deferred_synthetic_frames_.clear();
}

}  // namespace cc
