// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/container_full_width_behavior.h"

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// The virtual keyboard show/hide animation duration.
constexpr int kFullWidthKeyboardAnimationDurationMs = 100;

ContainerFullWidthBehavior::ContainerFullWidthBehavior(Delegate* delegate)
    : ContainerBehavior(delegate) {}

ContainerFullWidthBehavior::~ContainerFullWidthBehavior() = default;

ContainerType ContainerFullWidthBehavior::GetType() const {
  return ContainerType::kFullWidth;
}

void ContainerFullWidthBehavior::DoHidingAnimation(
    aura::Window* container,
    ::wm::ScopedHidingAnimationSettings* animation_settings) {
  animation_settings->layer_animation_settings()->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFullWidthKeyboardAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kFullWidthKeyboardAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(0.f);
}

void ContainerFullWidthBehavior::DoShowingAnimation(
    aura::Window* container,
    ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFullWidthKeyboardAnimationDurationMs));
  container->SetTransform(gfx::Transform());
  container->layer()->SetOpacity(1.0);
}

void ContainerFullWidthBehavior::InitializeShowAnimationStartingState(
    aura::Window* container) {
  SetCanonicalBounds(container, container->GetRootWindow()->bounds());

  gfx::Transform transform;
  transform.Translate(0, kFullWidthKeyboardAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

gfx::Rect ContainerFullWidthBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds_in_screen_coords) {
  gfx::Rect new_bounds;

  // Honors only the height of the request bounds
  const int keyboard_height = requested_bounds_in_screen_coords.height();

  new_bounds.set_y(display_bounds.bottom() - keyboard_height);
  new_bounds.set_height(keyboard_height);

  // If shelf is positioned on the left side of screen, x is not 0. In
  // FULL_WIDTH mode, the virtual keyboard should always align with the left
  // edge of the screen. So manually set x to the left side of the screen.
  new_bounds.set_x(display_bounds.x());
  new_bounds.set_width(display_bounds.width());

  return new_bounds;
}

bool ContainerFullWidthBehavior::IsOverscrollAllowed() const {
  // TODO(blakeo): The locked keyboard is essentially its own behavior type and
  // should be refactored as such. Then this will simply return 'true'.
  return delegate_ && !delegate_->IsKeyboardLocked();
}

void ContainerFullWidthBehavior::SavePosition(const gfx::Rect& keyboard_bounds,
                                              const gfx::Size& screen_size) {
  // No-op. Nothing to save.
}

bool ContainerFullWidthBehavior::HandlePointerEvent(
    const ui::LocatedEvent& event,
    const display::Display& current_display) {
  // No-op. Nothing special to do for pointer events.
  return false;
}

void ContainerFullWidthBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  const gfx::Rect new_keyboard_bounds =
      AdjustSetBoundsRequest(display_bounds, container->bounds());
  container->SetBounds(new_keyboard_bounds);
}

bool ContainerFullWidthBehavior::TextBlurHidesKeyboard() const {
  return !delegate_->IsKeyboardLocked();
}

void ContainerFullWidthBehavior::SetOccludedBounds(
    const gfx::Rect& occluded_bounds_in_window) {
  occluded_bounds_in_window_ = occluded_bounds_in_window;
}

gfx::Rect ContainerFullWidthBehavior::GetOccludedBounds(
    const gfx::Rect& visual_bounds_in_window) const {
  DCHECK(visual_bounds_in_window.Contains(occluded_bounds_in_window_));
  return occluded_bounds_in_window_.IsEmpty() ? visual_bounds_in_window
                                              : occluded_bounds_in_window_;
}

bool ContainerFullWidthBehavior::OccludedBoundsAffectWorkspaceLayout() const {
  return delegate_->IsKeyboardLocked();
}

void ContainerFullWidthBehavior::SetDraggableArea(const gfx::Rect& rect) {
  // Allow extension to call this function but does nothing here.
}

void ContainerFullWidthBehavior::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  // Allow extension to call this function but does nothing here.
}

}  //  namespace keyboard
