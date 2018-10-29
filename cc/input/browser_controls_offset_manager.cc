// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
      animation_initialized_(false),
      animation_start_value_(0.f),
      animation_stop_value_(0.f),
      animation_direction_(NO_ANIMATION),
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
  return client_->CurrentBrowserControlsShownRatio();
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
  return TopControlsShownRatio();
}

void BrowserControlsOffsetManager::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  DCHECK(!(constraints == BrowserControlsState::kShown &&
           current == BrowserControlsState::kHidden));
  DCHECK(!(constraints == BrowserControlsState::kHidden &&
           current == BrowserControlsState::kShown));

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
      current == BrowserControlsState::kHidden)
    final_shown_ratio = 0.f;
  if (final_shown_ratio == TopControlsShownRatio()) {
    ResetAnimations();
    return;
  }

  if (animate) {
    SetupAnimation(final_shown_ratio ? SHOWING_CONTROLS : HIDING_CONTROLS);
  } else {
    ResetAnimations();
    client_->SetCurrentBrowserControlsShownRatio(final_shown_ratio);
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
  float controls_height =
      TopControlsHeight() ? TopControlsHeight() : BottomControlsHeight();

  if (!controls_height)
    return pending_delta;

  if (pinch_gesture_active_)
    return pending_delta;

  if (permitted_state_ == BrowserControlsState::kShown && pending_delta.y() > 0)
    return pending_delta;
  else if (permitted_state_ == BrowserControlsState::kHidden &&
           pending_delta.y() < 0)
    return pending_delta;

  accumulated_scroll_delta_ += pending_delta.y();

  float old_top_offset = ContentTopOffset();
  float baseline_content_offset = TopControlsHeight()
      ? baseline_top_content_offset_ : baseline_bottom_content_offset_;
  client_->SetCurrentBrowserControlsShownRatio(
      (baseline_content_offset - accumulated_scroll_delta_) / controls_height);

  // If the controls are fully visible, treat the current position as the
  // new baseline even if the gesture didn't end.
  if (TopControlsShownRatio() == 1.f)
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
  if (!has_animation() || !client_->HaveRootScrollNode())
    return gfx::Vector2dF();

  if (!animation_initialized_) {
    // Setup the animation start and time here so that they use the same clock
    // as frame times. This is helpful for tests that mock time.
    animation_start_time_ = monotonic_time;
    animation_stop_time_ =
        animation_start_time_ +
        base::TimeDelta::FromMilliseconds(kShowHideMaxDurationMs);
    animation_initialized_ = true;
  }

  float old_offset = ContentTopOffset();
  float new_ratio = gfx::Tween::ClampedFloatValueBetween(
      monotonic_time, animation_start_time_, animation_start_value_,
      animation_stop_time_, animation_stop_value_);
  client_->SetCurrentBrowserControlsShownRatio(new_ratio);

  if (IsAnimationComplete(new_ratio))
    ResetAnimations();

  gfx::Vector2dF scroll_delta(0.f, ContentTopOffset() - old_offset);
  return scroll_delta;
}

void BrowserControlsOffsetManager::ResetAnimations() {
  animation_initialized_ = false;
  animation_start_time_ = base::TimeTicks();
  animation_start_value_ = 0.f;
  animation_stop_time_ = base::TimeTicks();
  animation_stop_value_ = 0.f;

  animation_direction_ = NO_ANIMATION;
}

void BrowserControlsOffsetManager::SetupAnimation(
    AnimationDirection direction) {
  DCHECK_NE(NO_ANIMATION, direction);
  DCHECK(direction != HIDING_CONTROLS || TopControlsShownRatio() > 0.f);
  DCHECK(direction != SHOWING_CONTROLS || TopControlsShownRatio() < 1.f);

  if (has_animation() && animation_direction_ == direction)
    return;

  if (!TopControlsHeight() && !BottomControlsHeight()) {
    client_->SetCurrentBrowserControlsShownRatio(
        direction == HIDING_CONTROLS ? 0.f : 1.f);
    return;
  }

  animation_start_value_ = TopControlsShownRatio();

  const float max_ending_ratio = (direction == SHOWING_CONTROLS ? 1 : -1);
  animation_stop_value_ = animation_start_value_ + max_ending_ratio;

  animation_direction_ = direction;
  client_->DidChangeBrowserControlsPosition();
}

void BrowserControlsOffsetManager::StartAnimationIfNecessary() {
  if (TopControlsShownRatio() == 0.f || TopControlsShownRatio() == 1.f)
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

bool BrowserControlsOffsetManager::IsAnimationComplete(float new_ratio) {
  return (animation_direction_ == SHOWING_CONTROLS && new_ratio >= 1.f) ||
         (animation_direction_ == HIDING_CONTROLS && new_ratio <= 0.f);
}

void BrowserControlsOffsetManager::ResetBaseline() {
  accumulated_scroll_delta_ = 0.f;
  baseline_top_content_offset_ = ContentTopOffset();
  baseline_bottom_content_offset_ = ContentBottomOffset();
}

}  // namespace cc
