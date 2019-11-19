// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/single_scrollbar_animation_controller_thinning.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

namespace {

float DistanceToScrollbarPart(const gfx::PointF& device_viewport_point,
                              const ScrollbarLayerImplBase& scrollbar,
                              const ScrollbarPart part) {
  gfx::RectF rect;
  if (part == ScrollbarPart::THUMB) {
    rect = gfx::RectF(gfx::Rect(scrollbar.ComputeExpandedThumbQuadRect()));
  } else {
    rect = gfx::RectF(gfx::Rect(scrollbar.bounds()));
  }

  gfx::RectF device_viewport_rect =
      MathUtil::MapClippedRect(scrollbar.ScreenSpaceTransform(), rect);

  return device_viewport_rect.ManhattanDistanceToPoint(device_viewport_point) /
         scrollbar.layer_tree_impl()->device_scale_factor();
}

}  // namespace

std::unique_ptr<SingleScrollbarAnimationControllerThinning>
SingleScrollbarAnimationControllerThinning::Create(
    ElementId scroll_element_id,
    ScrollbarOrientation orientation,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta thinning_duration) {
  return base::WrapUnique(new SingleScrollbarAnimationControllerThinning(
      scroll_element_id, orientation, client, thinning_duration));
}

SingleScrollbarAnimationControllerThinning::
    SingleScrollbarAnimationControllerThinning(
        ElementId scroll_element_id,
        ScrollbarOrientation orientation,
        ScrollbarAnimationControllerClient* client,
        base::TimeDelta thinning_duration)
    : client_(client),
      is_animating_(false),
      scroll_element_id_(scroll_element_id),
      orientation_(orientation),
      captured_(false),
      mouse_is_over_scrollbar_thumb_(false),
      mouse_is_near_scrollbar_thumb_(false),
      mouse_is_near_scrollbar_track_(false),
      thickness_change_(NONE),
      thinning_duration_(thinning_duration) {
  ApplyThumbThicknessScale(kIdleThicknessScale);
}

ScrollbarLayerImplBase*
SingleScrollbarAnimationControllerThinning::GetScrollbar() const {
  for (ScrollbarLayerImplBase* scrollbar :
       client_->ScrollbarsFor(scroll_element_id_)) {
    DCHECK(scrollbar->is_overlay_scrollbar());

    if (scrollbar->orientation() == orientation_)
      return scrollbar;
  }

  return nullptr;
}

bool SingleScrollbarAnimationControllerThinning::Animate(base::TimeTicks now) {
  if (!is_animating_)
    return false;

  if (last_awaken_time_.is_null())
    last_awaken_time_ = now;

  float progress = AnimationProgressAtTime(now);
  RunAnimationFrame(progress);

  return true;
}

float SingleScrollbarAnimationControllerThinning::AnimationProgressAtTime(
    base::TimeTicks now) {
  base::TimeDelta delta = now - last_awaken_time_;
  float progress = delta.InSecondsF() / Duration().InSecondsF();
  return base::ClampToRange(progress, 0.0f, 1.0f);
}

const base::TimeDelta& SingleScrollbarAnimationControllerThinning::Duration() {
  return thinning_duration_;
}

void SingleScrollbarAnimationControllerThinning::RunAnimationFrame(
    float progress) {
  if (captured_)
    return;

  ApplyThumbThicknessScale(ThumbThicknessScaleAt(progress));

  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f) {
    StopAnimation();
    thickness_change_ = NONE;
  }
}

void SingleScrollbarAnimationControllerThinning::StartAnimation() {
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsAnimateForScrollbarAnimation();
}

void SingleScrollbarAnimationControllerThinning::StopAnimation() {
  is_animating_ = false;
}

void SingleScrollbarAnimationControllerThinning::DidMouseDown() {
  if (!mouse_is_over_scrollbar_thumb_)
    return;

  StopAnimation();
  captured_ = true;
  ApplyThumbThicknessScale(1.f);
}

void SingleScrollbarAnimationControllerThinning::DidMouseUp() {
  if (!captured_)
    return;

  captured_ = false;
  StopAnimation();

  if (!mouse_is_near_scrollbar_thumb_) {
    thickness_change_ = DECREASE;
    StartAnimation();
  } else {
    thickness_change_ = NONE;
  }
}

void SingleScrollbarAnimationControllerThinning::DidMouseLeave() {
  if (!mouse_is_over_scrollbar_thumb_ && !mouse_is_near_scrollbar_thumb_)
    return;

  mouse_is_over_scrollbar_thumb_ = false;
  mouse_is_near_scrollbar_thumb_ = false;
  mouse_is_near_scrollbar_track_ = false;

  if (captured_)
    return;

  thickness_change_ = DECREASE;
  StartAnimation();
}

void SingleScrollbarAnimationControllerThinning::DidMouseMove(
    const gfx::PointF& device_viewport_point) {
  ScrollbarLayerImplBase* scrollbar = GetScrollbar();

  if (!scrollbar)
    return;

  float distance_to_scrollbar_track =
      DistanceToScrollbarPart(device_viewport_point, *scrollbar,
                              ScrollbarPart::TRACK_BUTTONS_TICKMARKS);
  float distance_to_scrollbar_thumb = DistanceToScrollbarPart(
      device_viewport_point, *scrollbar, ScrollbarPart::THUMB);

  mouse_is_near_scrollbar_track_ =
      distance_to_scrollbar_track <
      ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

  bool mouse_is_over_scrollbar_thumb = distance_to_scrollbar_thumb == 0.0f;
  bool mouse_is_near_scrollbar_thumb =
      distance_to_scrollbar_thumb < kMouseMoveDistanceToTriggerExpand;

  if (!captured_ &&
      mouse_is_near_scrollbar_thumb != mouse_is_near_scrollbar_thumb_) {
    thickness_change_ = mouse_is_near_scrollbar_thumb ? INCREASE : DECREASE;
    StartAnimation();
  }
  mouse_is_near_scrollbar_thumb_ = mouse_is_near_scrollbar_thumb;
  mouse_is_over_scrollbar_thumb_ = mouse_is_over_scrollbar_thumb;
}

float SingleScrollbarAnimationControllerThinning::ThumbThicknessScaleAt(
    float progress) {
  if (thickness_change_ == NONE)
    return mouse_is_near_scrollbar_thumb_ ? 1.f : kIdleThicknessScale;
  float factor = thickness_change_ == INCREASE ? progress : (1.f - progress);
  return ((1.f - kIdleThicknessScale) * factor) + kIdleThicknessScale;
}

float SingleScrollbarAnimationControllerThinning::AdjustScale(
    float new_value,
    float current_value,
    AnimationChange animation_change,
    float min_value,
    float max_value) {
  float result;
  if (animation_change == INCREASE && current_value > new_value)
    result = current_value;
  else if (animation_change == DECREASE && current_value < new_value)
    result = current_value;
  else
    result = new_value;
  if (result > max_value)
    return max_value;
  if (result < min_value)
    return min_value;
  return result;
}

void SingleScrollbarAnimationControllerThinning::UpdateThumbThicknessScale() {
  StopAnimation();
  ApplyThumbThicknessScale(
      mouse_is_near_scrollbar_thumb_ ? 1.f : kIdleThicknessScale);
}

void SingleScrollbarAnimationControllerThinning::ApplyThumbThicknessScale(
    float thumb_thickness_scale) {
  for (auto* scrollbar : client_->ScrollbarsFor(scroll_element_id_)) {
    if (scrollbar->orientation() != orientation_)
      continue;
    DCHECK(scrollbar->is_overlay_scrollbar());

    float scale = AdjustScale(thumb_thickness_scale,
                              scrollbar->thumb_thickness_scale_factor(),
                              thickness_change_, kIdleThicknessScale, 1);

    scrollbar->SetThumbThicknessScaleFactor(scale);
  }
}

}  // namespace cc
