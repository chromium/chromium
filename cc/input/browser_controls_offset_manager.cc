// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {
// These constants were chosen empirically for their visually pleasant behavior.
// Contact tedchoc@chromium.org for questions about changing these values.
const int64_t kShowHideMaxDurationMs = 200;
}

// static
std::unique_ptr<BrowserControlsOffsetManager>
BrowserControlsOffsetManager::Create(BrowserControlsOffsetManagerClient* client,
                                     float controls_show_threshold,
                                     float controls_hide_threshold) {
  return base::WrapUnique(new BrowserControlsOffsetManager(
      client, controls_show_threshold, controls_hide_threshold));
}

BrowserControlsOffsetManager::BrowserControlsOffsetManager(
    BrowserControlsOffsetManagerClient* client,
    float controls_show_threshold,
    float controls_hide_threshold)
    : client_(client),
      permitted_state_(BrowserControlsState::kBoth),
      accumulated_scroll_delta_(0.f),
      baseline_top_content_offset_(0.f),
      baseline_bottom_content_offset_(0.f),
      controls_show_threshold_(controls_hide_threshold),
      controls_hide_threshold_(controls_show_threshold),
      pinch_gesture_active_(false),
      constraint_changed_since_commit_(false) {
  CHECK(client_);
}

BrowserControlsOffsetManager::~BrowserControlsOffsetManager() = default;

float BrowserControlsOffsetManager::ControlsTopOffset() const {
  return ContentTopOffset() - TopControlsHeight();
}

float BrowserControlsOffsetManager::ContentTopOffset() const {
  return TopControlsHeight() > 0
      ? TopControlsShownRatio() * TopControlsHeight() : 0.0f;
}

float BrowserControlsOffsetManager::TopControlsShownRatio() const {
  return client_->CurrentTopControlsShownRatio();
}

float BrowserControlsOffsetManager::TopControlsHeight() const {
  return client_->TopControlsHeight();
}

float BrowserControlsOffsetManager::BottomControlsHeight() const {
  return client_->BottomControlsHeight();
}

float BrowserControlsOffsetManager::ContentBottomOffset() const {
  return BottomControlsHeight() > 0
      ? BottomControlsShownRatio() * BottomControlsHeight() : 0.0f;
}

float BrowserControlsOffsetManager::BottomControlsShownRatio() const {
  return client_->CurrentBottomControlsShownRatio();
}

void BrowserControlsOffsetManager::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  DCHECK(!(constraints == BrowserControlsState::kShown &&
           current == BrowserControlsState::kHidden));
  DCHECK(!(constraints == BrowserControlsState::kHidden &&
           current == BrowserControlsState::kShown));

  TRACE_EVENT2("cc", "BrowserControlsOffsetManager::UpdateBrowserControlsState",
               "constraints", static_cast<int>(constraints), "current",
               static_cast<int>(current));

  // If the constraints have changed we need to inform Blink about it since
  // that'll affect main thread scrolling as well as layout.
  if (permitted_state_ != constraints) {
    constraint_changed_since_commit_ = true;
    client_->SetNeedsCommit();
  }

  permitted_state_ = constraints;

  // Don't do anything if it doesn't matter which state the controls are in.
  if (constraints == BrowserControlsState::kBoth &&
      current == BrowserControlsState::kBoth)
    return;

  // Don't do anything if there is no change in offset.
  float final_shown_ratio = 1.f;
  if (constraints == BrowserControlsState::kHidden ||
      current == BrowserControlsState::kHidden) {
    final_shown_ratio = 0.f;
  }
  if (final_shown_ratio == TopControlsShownRatio()) {
    TRACE_EVENT_INSTANT0("cc", "Ratios Unchanged", TRACE_EVENT_SCOPE_THREAD);
    ResetAnimations();
    return;
  }

  if (animate) {
    SetupAnimation(final_shown_ratio ? SHOWING_CONTROLS : HIDING_CONTROLS);
  } else {
    ResetAnimations();
    client_->SetCurrentBrowserControlsShownRatio(final_shown_ratio,
                                                 final_shown_ratio);
  }
}

BrowserControlsState BrowserControlsOffsetManager::PullConstraintForMainThread(
    bool* out_changed_since_commit) {
  DCHECK(out_changed_since_commit);
  *out_changed_since_commit = constraint_changed_since_commit_;
  constraint_changed_since_commit_ = false;
  return permitted_state_;
}

void BrowserControlsOffsetManager::ScrollBegin() {
  if (pinch_gesture_active_)
    return;

  ResetAnimations();
  ResetBaseline();
}

gfx::Vector2dF BrowserControlsOffsetManager::ScrollBy(
    const gfx::Vector2dF& pending_delta) {
  // If one or both of the top/bottom controls are showing, the shown ratio
  // needs to be computed.
  if (!TopControlsHeight() && !BottomControlsHeight())
    return pending_delta;

  if (pinch_gesture_active_)
    return pending_delta;

  if (permitted_state_ == BrowserControlsState::kShown && pending_delta.y() > 0)
    return pending_delta;
  else if (permitted_state_ == BrowserControlsState::kHidden &&
           pending_delta.y() < 0)
    return pending_delta;

  accumulated_scroll_delta_ += pending_delta.y();

  // We want to base our calculations on top or bottom controls. After consuming
  // the scroll delta, we will calculate a shown ratio for the controls. The
  // top controls have the priority because they need to visually be in sync
  // with the web contents.
  bool base_on_top_controls = TopControlsHeight();

  float old_top_offset = ContentTopOffset();
  float baseline_content_offset = base_on_top_controls
                                      ? baseline_top_content_offset_
                                      : baseline_bottom_content_offset_;
  // The top and bottom controls ratios can be calculated independently.
  // However, we want the ratios to be equal when scrolling.
  float shown_ratio =
      (baseline_content_offset - accumulated_scroll_delta_) /
      (base_on_top_controls ? TopControlsHeight() : BottomControlsHeight());

  client_->SetCurrentBrowserControlsShownRatio(shown_ratio, shown_ratio);

  // If the controls are fully visible, treat the current position as the
  // new baseline even if the gesture didn't end.
  if (TopControlsShownRatio() == 1.f && BottomControlsShownRatio() == 1.f)
    ResetBaseline();

  ResetAnimations();

  // applied_delta will negate any scroll on the content if the top browser
  // controls are showing in favor of hiding the controls and resizing the
  // content. If the top controls have no height, the content should scroll
  // immediately.
  gfx::Vector2dF applied_delta(0.f, old_top_offset - ContentTopOffset());
  return pending_delta - applied_delta;
}

void BrowserControlsOffsetManager::ScrollEnd() {
  if (pinch_gesture_active_)
    return;

  StartAnimationIfNecessary();
}

void BrowserControlsOffsetManager::PinchBegin() {
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  StartAnimationIfNecessary();
}

void BrowserControlsOffsetManager::PinchEnd() {
  DCHECK(pinch_gesture_active_);
  // Pinch{Begin,End} will always occur within the scope of Scroll{Begin,End},
  // so return to a state expected by the remaining scroll sequence.
  pinch_gesture_active_ = false;
  ScrollBegin();
}

void BrowserControlsOffsetManager::MainThreadHasStoppedFlinging() {
  StartAnimationIfNecessary();
}

gfx::Vector2dF BrowserControlsOffsetManager::Animate(
    base::TimeTicks monotonic_time) {
  if (!HasAnimation() || !client_->HaveRootScrollNode())
    return gfx::Vector2dF();

  float old_offset = ContentTopOffset();
  float new_top_ratio = top_controls_animation_.Tick(monotonic_time);
  if (new_top_ratio < 0)
    new_top_ratio = TopControlsShownRatio();
  float new_bottom_ratio = bottom_controls_animation_.Tick(monotonic_time);
  if (new_bottom_ratio < 0)
    new_bottom_ratio = BottomControlsShownRatio();
  client_->SetCurrentBrowserControlsShownRatio(new_top_ratio, new_bottom_ratio);

  gfx::Vector2dF scroll_delta(0.f, ContentTopOffset() - old_offset);
  return scroll_delta;
}

bool BrowserControlsOffsetManager::HasAnimation() {
  return top_controls_animation_.IsInitialized() ||
         bottom_controls_animation_.IsInitialized();
}

void BrowserControlsOffsetManager::ResetAnimations() {
  top_controls_animation_.Reset();
  bottom_controls_animation_.Reset();
}

void BrowserControlsOffsetManager::SetupAnimation(
    AnimationDirection direction) {
  DCHECK_NE(NO_ANIMATION, direction);
  DCHECK(direction != HIDING_CONTROLS || TopControlsShownRatio() > 0.f);
  DCHECK(direction != SHOWING_CONTROLS || TopControlsShownRatio() < 1.f);

  if (top_controls_animation_.IsInitialized() &&
      top_controls_animation_.Direction() == direction &&
      bottom_controls_animation_.IsInitialized() &&
      bottom_controls_animation_.Direction() == direction)
    return;

  if (!TopControlsHeight() && !BottomControlsHeight()) {
    float ratio = direction == HIDING_CONTROLS ? 0.f : 1.f;
    client_->SetCurrentBrowserControlsShownRatio(ratio, ratio);
    return;
  }

  // Providing artificially larger/smaller stop ratios to make the animation
  // faster if the start ratio is closer to stop ratio.
  const float max_stop_ratio = direction == SHOWING_CONTROLS ? 1 : -1;
  float top_start_ratio = TopControlsShownRatio();
  float top_stop_ratio = top_start_ratio + max_stop_ratio;
  top_controls_animation_.Initialize(direction, top_start_ratio,
                                     top_stop_ratio);

  float bottom_start_ratio = BottomControlsShownRatio();
  float bottom_stop_ratio = bottom_start_ratio + max_stop_ratio;
  bottom_controls_animation_.Initialize(direction, bottom_start_ratio,
                                        bottom_stop_ratio);

  client_->DidChangeBrowserControlsPosition();
}

void BrowserControlsOffsetManager::StartAnimationIfNecessary() {
  if ((TopControlsShownRatio() == 0.f || TopControlsShownRatio() == 1.f) &&
      (BottomControlsShownRatio() == 0.f || BottomControlsShownRatio() == 1.f))
    return;

  if (TopControlsShownRatio() >= 1.f - controls_hide_threshold_) {
    // If we're showing so much that the hide threshold won't trigger, show.
    SetupAnimation(SHOWING_CONTROLS);
  } else if (TopControlsShownRatio() <= controls_show_threshold_) {
    // If we're showing so little that the show threshold won't trigger, hide.
    SetupAnimation(HIDING_CONTROLS);
  } else {
    // If we could be either showing or hiding, we determine which one to
    // do based on whether or not the total scroll delta was moving up or
    // down.
    SetupAnimation(accumulated_scroll_delta_ <= 0.f ? SHOWING_CONTROLS
                                                    : HIDING_CONTROLS);
  }
}

void BrowserControlsOffsetManager::ResetBaseline() {
  accumulated_scroll_delta_ = 0.f;
  baseline_top_content_offset_ = ContentTopOffset();
  baseline_bottom_content_offset_ = ContentBottomOffset();
}

// class Animation

void BrowserControlsOffsetManager::Animation::Initialize(
    AnimationDirection direction,
    float start_value,
    float stop_value) {
  direction_ = direction;
  start_value_ = start_value;
  stop_value_ = stop_value;
  initialized_ = true;
}

float BrowserControlsOffsetManager::Animation::Tick(
    base::TimeTicks monotonic_time) {
  if (!IsInitialized())
    return -1;

  if (!started_) {
    start_time_ = monotonic_time;
    stop_time_ =
        start_time_ + base::TimeDelta::FromMilliseconds(kShowHideMaxDurationMs);
    started_ = true;
  }

  float value = gfx::Tween::ClampedFloatValueBetween(
      monotonic_time, start_time_, start_value_, stop_time_, stop_value_);

  if (IsComplete(value)) {
    value = stop_value_;
    Reset();
  }
  return base::ClampToRange(value, min_value_, max_value_);
}

void BrowserControlsOffsetManager::Animation::SetBounds(float min, float max) {
  min_value_ = min;
  max_value_ = max;
}

void BrowserControlsOffsetManager::Animation::Reset() {
  started_ = false;
  initialized_ = false;
  start_time_ = base::TimeTicks();
  start_value_ = 0.f;
  stop_time_ = base::TimeTicks();
  stop_value_ = 0.f;
  direction_ = NO_ANIMATION;
}

bool BrowserControlsOffsetManager::Animation::IsComplete(float value) {
  return (direction_ == SHOWING_CONTROLS &&
          (value >= stop_value_ || value >= max_value_)) ||
         (direction_ == HIDING_CONTROLS &&
          (value <= stop_value_ || value <= min_value_));
}

}  // namespace cc
