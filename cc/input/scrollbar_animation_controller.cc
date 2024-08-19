// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

std::unique_ptr<ScrollbarAnimationController>
ScrollbarAnimationController::CreateScrollbarAnimationControllerAndroid(
    ElementId scroll_element_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_delay,
    base::TimeDelta fade_duration,
    float initial_opacity) {
  return base::WrapUnique(new ScrollbarAnimationController(
      scroll_element_id, client, fade_delay, fade_duration, initial_opacity));
}

std::unique_ptr<ScrollbarAnimationController>
ScrollbarAnimationController::CreateScrollbarAnimationControllerAuraOverlay(
    ElementId scroll_element_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_delay,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration,
    float initial_opacity,
    float idle_thickness_scale) {
  return base::WrapUnique(new ScrollbarAnimationController(
      scroll_element_id, client, fade_delay, fade_duration, thinning_duration,
      initial_opacity, idle_thickness_scale));
}

ScrollbarAnimationController::ScrollbarAnimationController(
    ElementId scroll_element_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_delay,
    base::TimeDelta fade_duration,
    float initial_opacity)
    : client_(client),
      fade_delay_(fade_delay),
      fade_duration_(fade_duration),
      need_trigger_scrollbar_fade_in_(false),
      is_animating_(false),
      animation_change_(AnimationChange::kNone),
      scroll_element_id_(scroll_element_id),
      opacity_(initial_opacity),
      show_scrollbars_on_scroll_gesture_(false),
      need_thinning_animation_(false),
      is_mouse_down_(false),
      tickmarks_showing_(false) {}

ScrollbarAnimationController::ScrollbarAnimationController(
    ElementId scroll_element_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_delay,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration,
    float initial_opacity,
    float idle_thickness_scale)
    : client_(client),
      fade_delay_(fade_delay),
      fade_duration_(fade_duration),
      need_trigger_scrollbar_fade_in_(false),
      is_animating_(false),
      animation_change_(AnimationChange::kNone),
      scroll_element_id_(scroll_element_id),
      opacity_(initial_opacity),
      show_scrollbars_on_scroll_gesture_(true),
      need_thinning_animation_(true),
      is_mouse_down_(false),
      tickmarks_showing_(false) {
  vertical_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_element_id, ScrollbarOrientation::kVertical, client,
      thinning_duration, idle_thickness_scale);
  horizontal_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_element_id, ScrollbarOrientation::kHorizontal, client,
      thinning_duration, idle_thickness_scale);
}

ScrollbarAnimationController::~ScrollbarAnimationController() = default;

ScrollbarSet ScrollbarAnimationController::Scrollbars() const {
  return client_->ScrollbarsFor(scroll_element_id_);
}

SingleScrollbarAnimationControllerThinning&
ScrollbarAnimationController::GetScrollbarAnimationController(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  if (orientation == ScrollbarOrientation::kVertical) {
    return *(vertical_controller_.get());
  } else {
    return *(horizontal_controller_.get());
  }
}

void ScrollbarAnimationController::StartAnimation() {
  DCHECK(animation_change_ != AnimationChange::kNone);
  delayed_scrollbar_animation_.Cancel();
  need_trigger_scrollbar_fade_in_ = false;
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsAnimateForScrollbarAnimation();
}

void ScrollbarAnimationController::StopAnimation() {
  delayed_scrollbar_animation_.Cancel();
  need_trigger_scrollbar_fade_in_ = false;
  is_animating_ = false;
  animation_change_ = AnimationChange::kNone;
}

void ScrollbarAnimationController::PostDelayedAnimation(
    AnimationChange animation_change) {
  // If fade duration is zero we are in a test environment and should not
  // animate.
  if (fade_duration_.is_zero()) {
    return;
  }

  animation_change_ = animation_change;
  delayed_scrollbar_animation_.Cancel();
  delayed_scrollbar_animation_.Reset(
      base::BindOnce(&ScrollbarAnimationController::StartAnimation,
                     weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarAnimationTask(
      delayed_scrollbar_animation_.callback(), fade_delay_);
}

bool ScrollbarAnimationController::Animate(base::TimeTicks now) {
  bool animated = false;

  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->CanScrollOrientation()) {
      scrollbar->SetOverlayScrollbarLayerOpacityAnimated(
          0, /*fade_out_animation=*/false);
    }
  }

  if (is_animating_) {
    DCHECK(animation_change_ != AnimationChange::kNone);
    if (last_awaken_time_.is_null())
      last_awaken_time_ = now;

    float progress = AnimationProgressAtTime(now);
    RunAnimationFrame(progress);

    if (is_animating_)
      client_->SetNeedsAnimateForScrollbarAnimation();
    animated = true;
  }

  if (need_thinning_animation_) {
    animated |= vertical_controller_->Animate(now);
    animated |= horizontal_controller_->Animate(now);
  }

  return animated;
}

float ScrollbarAnimationController::AnimationProgressAtTime(
    base::TimeTicks now) {
  const base::TimeDelta delta = now - last_awaken_time_;
  return std::clamp(static_cast<float>(delta / fade_duration_), 0.0f, 1.0f);
}

void ScrollbarAnimationController::RunAnimationFrame(float progress) {
  float opacity;

  DCHECK(animation_change_ != AnimationChange::kNone);
  if (animation_change_ == AnimationChange::kFadeIn) {
    opacity = std::max(progress, opacity_);
  } else {
    opacity = std::min(1.f - progress, opacity_);
  }

  ApplyOpacityToScrollbars(opacity);
  if (progress == 1.f)
    StopAnimation();
}

void ScrollbarAnimationController::DidScrollUpdate() {
  UpdateScrollbarState();
  if (need_thinning_animation_) {
    vertical_controller_->DidScrollUpdate();
    horizontal_controller_->DidScrollUpdate();
  }
}

void ScrollbarAnimationController::UpdateScrollbarState() {
  if (need_thinning_animation_ && Captured())
    return;

  StopAnimation();

  Show();

  // We don't fade out scrollbar if they need thinning animation (Aura
  // Overlay) and mouse is near or tickmarks show.
  if (need_thinning_animation_) {
    if (!MouseIsNearAnyScrollbar() && !tickmarks_showing_)
      PostDelayedAnimation(AnimationChange::kFadeOut);
  } else {
    PostDelayedAnimation(AnimationChange::kFadeOut);
  }
}

void ScrollbarAnimationController::WillUpdateScroll() {
  if (show_scrollbars_on_scroll_gesture_) {
    UpdateScrollbarState();
    if (need_thinning_animation_) {
      vertical_controller_->DidRequestShow();
      horizontal_controller_->DidRequestShow();
    }
  }
}

void ScrollbarAnimationController::DidRequestShow() {
  UpdateScrollbarState();
  if (need_thinning_animation_) {
    vertical_controller_->DidRequestShow();
    horizontal_controller_->DidRequestShow();
  }
}

void ScrollbarAnimationController::UpdateTickmarksVisibility(bool show) {
  if (!need_thinning_animation_)
    return;

  if (tickmarks_showing_ == show)
    return;

  tickmarks_showing_ = show;
  vertical_controller_->UpdateTickmarksVisibility(show);
  UpdateScrollbarState();
}

void ScrollbarAnimationController::DidMouseDown() {
  if (!need_thinning_animation_)
    return;

  is_mouse_down_ = true;

  if (ScrollbarsHidden()) {
    if (need_trigger_scrollbar_fade_in_) {
      delayed_scrollbar_animation_.Cancel();
      need_trigger_scrollbar_fade_in_ = false;
    }
    return;
  }

  vertical_controller_->DidMouseDown();
  horizontal_controller_->DidMouseDown();
}

void ScrollbarAnimationController::DidMouseUp() {
  if (!need_thinning_animation_)
    return;

  is_mouse_down_ = false;

  if (!Captured()) {
    if (MouseIsNearAnyScrollbar() && ScrollbarsHidden()) {
      PostDelayedAnimation(AnimationChange::kFadeIn);
      need_trigger_scrollbar_fade_in_ = true;
    }
    return;
  }

  vertical_controller_->DidMouseUp();
  horizontal_controller_->DidMouseUp();

  if (!MouseIsNearAnyScrollbar() && !ScrollbarsHidden() && !tickmarks_showing_)
    PostDelayedAnimation(AnimationChange::kFadeOut);
}

void ScrollbarAnimationController::DidMouseLeave() {
  if (!need_thinning_animation_)
    return;

  vertical_controller_->DidMouseLeave();
  horizontal_controller_->DidMouseLeave();

  delayed_scrollbar_animation_.Cancel();
  need_trigger_scrollbar_fade_in_ = false;

  if (ScrollbarsHidden() || Captured() || tickmarks_showing_)
    return;

  PostDelayedAnimation(AnimationChange::kFadeOut);
}

void ScrollbarAnimationController::DidMouseMove(
    const gfx::PointF& device_viewport_point) {
  if (!need_thinning_animation_)
    return;

  bool need_trigger_scrollbar_fade_in_before = need_trigger_scrollbar_fade_in_;

  vertical_controller_->DidMouseMove(device_viewport_point);
  horizontal_controller_->DidMouseMove(device_viewport_point);

  if (Captured() || tickmarks_showing_) {
    DCHECK(!ScrollbarsHidden());
    return;
  }

  if (ScrollbarsHidden()) {
    // Do not fade in scrollbar when user interacting with the content below
    // scrollbar. Fluent scrollbars never leave invisibility due to pointer
    // moves.
    if (is_mouse_down_ || client_->IsFluentOverlayScrollbar()) {
      return;
    }
    need_trigger_scrollbar_fade_in_ = MouseIsNearAnyScrollbar();
    if (need_trigger_scrollbar_fade_in_before !=
        need_trigger_scrollbar_fade_in_) {
      if (need_trigger_scrollbar_fade_in_) {
        PostDelayedAnimation(AnimationChange::kFadeIn);
      } else {
        delayed_scrollbar_animation_.Cancel();
      }
    }
  } else {
    if (MouseIsNearAnyScrollbar()) {
      Show();
      StopAnimation();
    } else if (!is_animating_ || client_->IsFluentOverlayScrollbar()) {
      PostDelayedAnimation(AnimationChange::kFadeOut);
    }
  }
}

bool ScrollbarAnimationController::MouseIsOverScrollbarThumb(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(orientation)
      .mouse_is_over_scrollbar_thumb();
}

bool ScrollbarAnimationController::MouseIsNearScrollbarThumb(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(orientation)
      .mouse_is_near_scrollbar_thumb();
}

bool ScrollbarAnimationController::MouseIsNearScrollbar(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(orientation).mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::MouseIsNearAnyScrollbar() const {
  DCHECK(need_thinning_animation_);
  return vertical_controller_->mouse_is_near_scrollbar() ||
         horizontal_controller_->mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::ScrollbarsHidden() const {
  return opacity_ == 0.0f;
}

bool ScrollbarAnimationController::Captured() const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
             .captured() ||
         GetScrollbarAnimationController(ScrollbarOrientation::kHorizontal)
             .captured();
}

void ScrollbarAnimationController::Show() {
  delayed_scrollbar_animation_.Cancel();
  ApplyOpacityToScrollbars(1.0f);
}

void ScrollbarAnimationController::ApplyOpacityToScrollbars(float opacity) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    DCHECK(scrollbar->is_overlay_scrollbar());
    float effective_opacity = scrollbar->CanScrollOrientation() ? opacity : 0;
    scrollbar->SetOverlayScrollbarLayerOpacityAnimated(
        effective_opacity,
        /*fade_out_animation=*/animation_change_ == AnimationChange::kFadeOut);
  }

  bool previously_visible_ = opacity_ > 0.0f;
  bool currently_visible = opacity > 0.0f;

  if (opacity_ != opacity) {
    client_->SetNeedsRedrawForScrollbarAnimation();
  }

  opacity_ = opacity;

  if (previously_visible_ != currently_visible) {
    client_->DidChangeScrollbarVisibility();
    visibility_changed_ = true;
  }
}

}  // namespace cc
