// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/single_scrollbar_animation_controller_thinning.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

namespace {

float DistanceToRect(const gfx::PointF& device_viewport_point,
                     const ScrollbarLayerImplBase& scrollbar,
                     const gfx::Rect& rect) {
  gfx::RectF device_viewport_rect = MathUtil::MapClippedRect(
      scrollbar.ScreenSpaceTransform(), gfx::RectF(rect));

  return device_viewport_rect.ManhattanDistanceToPoint(device_viewport_point) /
         scrollbar.layer_tree_impl()->device_scale_factor();
}

float DistanceToScrollbar(const gfx::PointF& device_viewport_point,
                          const ScrollbarLayerImplBase& scrollbar) {
  return DistanceToRect(device_viewport_point, scrollbar,
                        gfx::Rect(scrollbar.bounds()));
}

float DistanceToScrollbarThumb(const gfx::PointF& device_viewport_point,
                               const ScrollbarLayerImplBase& scrollbar) {
  return DistanceToRect(device_viewport_point, scrollbar,
                        scrollbar.ComputeHitTestableExpandedThumbQuadRect());
}

}  // namespace

std::unique_ptr<SingleScrollbarAnimationControllerThinning>
SingleScrollbarAnimationControllerThinning::Create(
    ElementId scroll_element_id,
    ScrollbarOrientation orientation,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta thinning_duration,
    float idle_thickness_scale) {
  return base::WrapUnique(new SingleScrollbarAnimationControllerThinning(
      scroll_element_id, orientation, client, thinning_duration,
      idle_thickness_scale));
}

SingleScrollbarAnimationControllerThinning::
    SingleScrollbarAnimationControllerThinning(
        ElementId scroll_element_id,
        ScrollbarOrientation orientation,
        ScrollbarAnimationControllerClient* client,
        base::TimeDelta thinning_duration,
        float idle_thickness_scale)
    : client_(client),
      scroll_element_id_(scroll_element_id),
      orientation_(orientation),
      thinning_duration_(thinning_duration),
      idle_thickness_scale_(idle_thickness_scale) {
  ApplyThumbThicknessScale(idle_thickness_scale_);
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
  // In tests, there may be no duration; snap to the end in such a case.
  if (thinning_duration_.is_zero())
    return 1.0f;

  const base::TimeDelta delta = now - last_awaken_time_;
  return std::clamp(static_cast<float>(delta / thinning_duration_), 0.0f, 1.0f);
}

void SingleScrollbarAnimationControllerThinning::RunAnimationFrame(
    float progress) {
  if (captured_)
    return;

  ApplyThumbThicknessScale(ThumbThicknessScaleAt(progress));

  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f) {
    StopAnimation();
    thickness_change_ = AnimationChange::kNone;
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

void SingleScrollbarAnimationControllerThinning::DidScrollUpdate() {
  if (captured_ || !mouse_is_near_scrollbar_) {
    return;
  }

  CalculateThicknessShouldChange(device_viewport_last_pointer_location_);
  // If scrolling with the pointer on top of the scrollbar, force the scrollbar
  // to expand.
  if (thickness_change_ == AnimationChange::kNone) {
    UpdateThumbThicknessScale();
  }
}

void SingleScrollbarAnimationControllerThinning::DidMouseDown() {
  // When invisible, Fluent scrollbars are disabled and their thumb has no
  // dimensions, which causes mouse_is_over_scrollbar_thumb_ to always be false.
  // This check updates the thumb variable to cover the cases where you mouse
  // over the invisible thumb, make it appear by some mechanism (tickmarks,
  // scrolling, etc.) and press mouse down without moving your pointer.
  if (client_->IsFluentOverlayScrollbar() && !mouse_is_over_scrollbar_thumb_) {
    ScrollbarLayerImplBase* scrollbar = GetScrollbar();
    if (scrollbar) {
      const float distance_to_scrollbar_thumb = DistanceToScrollbarThumb(
          device_viewport_last_pointer_location_, *scrollbar);
      mouse_is_over_scrollbar_thumb_ = distance_to_scrollbar_thumb == 0.0f;
    }
  }

  if (!mouse_is_over_scrollbar_thumb_)
    return;

  captured_ = true;
  UpdateThumbThicknessScale();
}

void SingleScrollbarAnimationControllerThinning::DidMouseUp() {
  if (!captured_)
    return;

  captured_ = false;
  StopAnimation();

  // On mouse up, Fluent scrollbars go straight to the scrollbar disappearance
  // animation (via ScrollbarAnimationController) without queueing a thinning
  // animation.
  const bool thickness_should_decrease =
      !client_->IsFluentOverlayScrollbar() && !mouse_is_near_scrollbar_thumb_;

  if (thickness_should_decrease) {
    thickness_change_ = AnimationChange::kDecrease;
    StartAnimation();
  } else {
    thickness_change_ = AnimationChange::kNone;
  }
}

void SingleScrollbarAnimationControllerThinning::DidMouseLeave() {
  mouse_is_over_scrollbar_thumb_ = false;
  mouse_is_near_scrollbar_thumb_ = false;
  mouse_is_near_scrollbar_ = false;

  if (captured_) {
    return;
  }

  // If fully expanded, Fluent scrollbars don't queue a thinning animation and
  // let the ScrollbarAnimationController make the scrollbars disappear.
  if (client_->IsFluentOverlayScrollbar() &&
      thickness_change_ == AnimationChange::kNone) {
    return;
  }

  thickness_change_ = AnimationChange::kDecrease;
  StartAnimation();
}

void SingleScrollbarAnimationControllerThinning::DidMouseMove(
    const gfx::PointF& device_viewport_point) {
  CalculateThicknessShouldChange(device_viewport_point);
  device_viewport_last_pointer_location_ = device_viewport_point;
}

void SingleScrollbarAnimationControllerThinning::CalculateThicknessShouldChange(
    const gfx::PointF& device_viewport_point) {
  ScrollbarLayerImplBase* scrollbar = GetScrollbar();

  if (!scrollbar)
    return;

  const float distance_to_scrollbar =
      DistanceToScrollbar(device_viewport_point, *scrollbar);
  const float distance_to_scrollbar_thumb =
      DistanceToScrollbarThumb(device_viewport_point, *scrollbar);

  const bool mouse_is_near_scrollbar =
      distance_to_scrollbar <= MouseMoveDistanceToTriggerFadeIn();

  const bool mouse_is_over_scrollbar_thumb =
      distance_to_scrollbar_thumb == 0.0f;
  const bool mouse_is_near_scrollbar_thumb =
      distance_to_scrollbar_thumb <= MouseMoveDistanceToTriggerExpand();
  bool thickness_should_change;
  if (client_->IsFluentOverlayScrollbar()) {
    const bool is_visible = scrollbar->OverlayScrollbarOpacity() > 0.f;
    const bool moved_over_scrollbar =
        mouse_is_near_scrollbar_ != mouse_is_near_scrollbar;
    const bool mouse_far_from_scrollbar =
        (!mouse_is_near_scrollbar &&
         thickness_change_ == AnimationChange::kNone);
    // On mouse move Fluent scrollbars will queue a thinning animation iff the
    // scrollbar is visible and either the mouse has moved over the scrollbar
    // (increase thickness) or the mouse has moved far away from the scrollbar
    // and there is no previously queued animation (decreasse thickness).
    // If tickmarks are shown, the scrollbars should be and should remain in
    // Full mode.
    thickness_should_change =
        !tickmarks_showing_ && is_visible &&
        (moved_over_scrollbar || mouse_far_from_scrollbar);
  } else {
    thickness_should_change =
        (mouse_is_near_scrollbar_thumb_ != mouse_is_near_scrollbar_thumb);
  }

  if (!captured_ && thickness_should_change) {
    const bool thickness_should_increase = client_->IsFluentOverlayScrollbar()
                                               ? mouse_is_near_scrollbar
                                               : mouse_is_near_scrollbar_thumb;
    thickness_change_ = thickness_should_increase ? AnimationChange::kIncrease
                                                  : AnimationChange::kDecrease;
    StartAnimation();
  }

  mouse_is_near_scrollbar_ = mouse_is_near_scrollbar;
  mouse_is_near_scrollbar_thumb_ = mouse_is_near_scrollbar_thumb;
  mouse_is_over_scrollbar_thumb_ = mouse_is_over_scrollbar_thumb;
}

float SingleScrollbarAnimationControllerThinning::ThumbThicknessScaleAt(
    float progress) const {
  CHECK_NE(thickness_change_, AnimationChange::kNone);
  float factor = thickness_change_ == AnimationChange::kIncrease
                     ? progress
                     : (1.f - progress);
  return ((1.f - idle_thickness_scale_) * factor) + idle_thickness_scale_;
}

float SingleScrollbarAnimationControllerThinning::AdjustScale(
    float new_value,
    float current_value,
    AnimationChange animation_change,
    float min_value,
    float max_value) {
  float result;
  if (animation_change == AnimationChange::kIncrease &&
      current_value > new_value) {
    result = current_value;
  } else if (animation_change == AnimationChange::kDecrease &&
             current_value < new_value) {
    result = current_value;
  } else {
    result = new_value;
  }
  if (result > max_value)
    return max_value;
  if (result < min_value)
    return min_value;
  return result;
}

float SingleScrollbarAnimationControllerThinning::
    CurrentForcedThumbThicknessScale() const {
  bool thumb_should_be_expanded;
  if (client_->IsFluentOverlayScrollbar()) {
    thumb_should_be_expanded = mouse_is_near_scrollbar_ || tickmarks_showing_;
  } else {
    thumb_should_be_expanded = mouse_is_near_scrollbar_thumb_;
  }
  thumb_should_be_expanded |= captured_;
  return thumb_should_be_expanded ? 1.f : idle_thickness_scale_;
}

void SingleScrollbarAnimationControllerThinning::UpdateThumbThicknessScale() {
  StopAnimation();
  ApplyThumbThicknessScale(CurrentForcedThumbThicknessScale());
}

void SingleScrollbarAnimationControllerThinning::DidRequestShow() {
  if (thickness_change_ == AnimationChange::kNone) {
    UpdateThumbThicknessScale();
  }
}

void SingleScrollbarAnimationControllerThinning::ApplyThumbThicknessScale(
    float thumb_thickness_scale) {
  for (auto* scrollbar : client_->ScrollbarsFor(scroll_element_id_)) {
    if (scrollbar->orientation() != orientation_)
      continue;
    DCHECK(scrollbar->is_overlay_scrollbar());

    float scale = AdjustScale(thumb_thickness_scale,
                              scrollbar->thumb_thickness_scale_factor(),
                              thickness_change_, idle_thickness_scale_, 1);

    scrollbar->SetThumbThicknessScaleFactor(scale);
  }
}

void SingleScrollbarAnimationControllerThinning::UpdateTickmarksVisibility(
    bool show) {
  tickmarks_showing_ = show;
  if (show) {
    UpdateThumbThicknessScale();
  }
}

float SingleScrollbarAnimationControllerThinning::
    MouseMoveDistanceToTriggerExpand() {
  return client_->IsFluentOverlayScrollbar() ? 0.0f : 25.0f;
}

float SingleScrollbarAnimationControllerThinning::
    MouseMoveDistanceToTriggerFadeIn() {
  return client_->IsFluentOverlayScrollbar() ? 0.0f : 30.0f;
}

}  // namespace cc
