// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/container_floating_behavior.h"

#include <memory>
#include <optional>

#include "ash/keyboard/ui/display_util.h"
#include "ash/keyboard/ui/drag_descriptor.h"
#include "base/numerics/safe_conversions.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// The virtual keyboard show/hide animation durations.
constexpr auto kShowAnimationDuration = base::Milliseconds(200);
constexpr auto kHideAnimationDuration = base::Milliseconds(100);

// Distance the keyboard moves during the animation
constexpr int kAnimationDistance = 30;

ContainerFloatingBehavior::ContainerFloatingBehavior(Delegate* delegate)
    : ContainerBehavior(delegate) {}

ContainerFloatingBehavior::~ContainerFloatingBehavior() = default;

ContainerType ContainerFloatingBehavior::GetType() const {
  return ContainerType::kFloating;
}

void ContainerFloatingBehavior::DoHidingAnimation(
    aura::Window* container,
    ::wm::ScopedHidingAnimationSettings* animation_settings) {
  animation_settings->layer_animation_settings()->SetTransitionDuration(
      kHideAnimationDuration);
  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(0.f);
}

void ContainerFloatingBehavior::DoShowingAnimation(
    aura::Window* container,
    ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings->SetTransitionDuration(kShowAnimationDuration);

  container->SetTransform(gfx::Transform());
  container->layer()->SetOpacity(1.0);
}

void ContainerFloatingBehavior::InitializeShowAnimationStartingState(
    aura::Window* container) {
  aura::Window* root_window = container->GetRootWindow();

  SetCanonicalBounds(container, root_window->bounds());

  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

gfx::Rect ContainerFloatingBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds_in_screen) {
  gfx::Rect keyboard_bounds_in_screen = ContainKeyboardToDisplayBounds(
      requested_bounds_in_screen, display_bounds);
  SavePosition(keyboard_bounds_in_screen, display_bounds.size());
  return keyboard_bounds_in_screen;
}

void ContainerFloatingBehavior::SavePosition(
    const gfx::Rect& keyboard_bounds_in_screen,
    const gfx::Size& screen_size) {
  int left_distance = keyboard_bounds_in_screen.x();
  int right_distance = screen_size.width() - keyboard_bounds_in_screen.right();
  int top_distance = keyboard_bounds_in_screen.y();
  int bottom_distance =
      screen_size.height() - keyboard_bounds_in_screen.bottom();

  double available_width = left_distance + right_distance;
  double available_height = top_distance + bottom_distance;

  if (!default_position_in_screen_) {
    default_position_in_screen_ = std::make_unique<KeyboardPosition>();
  }

  default_position_in_screen_->left_padding_allotment_ratio =
      left_distance / available_width;
  default_position_in_screen_->top_padding_allotment_ratio =
      top_distance / available_height;
}

gfx::Rect ContainerFloatingBehavior::ConvertAreaInKeyboardToScreenBounds(
    const gfx::Rect& area_in_keyboard_window,
    const gfx::Rect& keyboard_window_bounds_in_display) const {
  gfx::Point origin_in_screen(
      keyboard_window_bounds_in_display.x() + area_in_keyboard_window.x(),
      keyboard_window_bounds_in_display.y() + area_in_keyboard_window.y());
  return gfx::Rect(origin_in_screen, area_in_keyboard_window.size());
}

gfx::Rect ContainerFloatingBehavior::GetBoundsWithinDisplay(
    const gfx::Rect& bounds,
    const gfx::Rect& display_bounds) const {
  gfx::Rect new_bounds = bounds;

  if (bounds.x() < display_bounds.x()) {
    new_bounds.set_origin(gfx::Point(display_bounds.x(), new_bounds.y()));
  }
  if (bounds.right() >= display_bounds.right()) {
    new_bounds.set_origin(
        gfx::Point(display_bounds.right() - bounds.width(), new_bounds.y()));
  }
  if (bounds.y() < display_bounds.y()) {
    new_bounds.set_origin(gfx::Point(new_bounds.x(), display_bounds.y()));
  }
  if (bounds.bottom() >= display_bounds.bottom()) {
    new_bounds.set_origin(gfx::Point(
        new_bounds.x(), display_bounds.bottom() - new_bounds.height()));
  }

  return new_bounds;
}

gfx::Rect ContainerFloatingBehavior::ContainKeyboardToDisplayBounds(
    const gfx::Rect& keyboard_window_bounds_in_screen,
    const gfx::Rect& display_bounds) const {
  if (!area_in_window_to_remain_on_screen_) {
    return GetBoundsWithinDisplay(keyboard_window_bounds_in_screen,
                                  display_bounds);
  }

  // This area is relative to the origin of the keyboard window not the
  // screen.
  gfx::Rect inner_area_of_keyboard_window =
      *area_in_window_to_remain_on_screen_;

  gfx::Rect area_to_remain_on_display = ConvertAreaInKeyboardToScreenBounds(
      inner_area_of_keyboard_window, keyboard_window_bounds_in_screen);

  gfx::Rect area_constrained_to_display =
      GetBoundsWithinDisplay(area_to_remain_on_display, display_bounds);

  // We need to calculate the new keyboard window bounds in this method,
  // and at the moment we have constrained only an area inside the window
  // to the display, not the entire keyboard window. So now we must
  // derive the containing keyboard window bounds from this constrained
  // inner area.
  gfx::Point containing_keyboard_window_origin(
      area_constrained_to_display.x() - inner_area_of_keyboard_window.x(),
      area_constrained_to_display.y() - inner_area_of_keyboard_window.y());

  return gfx::Rect(containing_keyboard_window_origin,
                   keyboard_window_bounds_in_screen.size());
}

bool ContainerFloatingBehavior::IsOverscrollAllowed() const {
  return false;
}

gfx::Point ContainerFloatingBehavior::GetPositionForShowingKeyboard(
    const gfx::Size& keyboard_size,
    const gfx::Rect& display_bounds) const {
  // Start with the last saved position
  gfx::Point top_left_offset;
  KeyboardPosition* position = default_position_in_screen_.get();
  if (position == nullptr) {
    // If there is none, center the keyboard along the bottom of the screen.
    top_left_offset.set_x(display_bounds.width() - keyboard_size.width() -
                          kDefaultDistanceFromScreenRight);
    top_left_offset.set_y(display_bounds.height() - keyboard_size.height() -
                          kDefaultDistanceFromScreenBottom);
  } else {
    double left = (display_bounds.width() - keyboard_size.width()) *
                  position->left_padding_allotment_ratio;
    double top = (display_bounds.height() - keyboard_size.height()) *
                 position->top_padding_allotment_ratio;
    top_left_offset.set_x(base::ClampFloor(left));
    top_left_offset.set_y(base::ClampFloor(top));
  }

  // Make sure that this location is valid according to the current size of the
  // screen.
  gfx::Rect keyboard_bounds =
      gfx::Rect(top_left_offset.x() + display_bounds.x(),
                top_left_offset.y() + display_bounds.y(), keyboard_size.width(),
                keyboard_size.height());

  gfx::Rect valid_keyboard_bounds =
      ContainKeyboardToDisplayBounds(keyboard_bounds, display_bounds);

  return valid_keyboard_bounds.origin();
}

bool ContainerFloatingBehavior::HandlePointerEvent(
    const ui::LocatedEvent& event,
    const display::Display& current_display) {
  const gfx::Vector2d kb_offset(base::ClampFloor(event.x()),
                                base::ClampFloor(event.y()));

  const gfx::Rect& keyboard_bounds_in_screen = delegate_->GetBoundsInScreen();

  // Don't handle events if this runs in a partially initialized state.
  if (keyboard_bounds_in_screen.height() <= 0)
    return false;

  ui::PointerId pointer_id = ui::kPointerIdMouse;
  if (event.IsTouchEvent()) {
    const ui::TouchEvent* te = event.AsTouchEvent();
    pointer_id = te->pointer_details().id;
  }

  const ui::EventType type = event.type();
  switch (type) {
    case ui::EventType::kTouchPressed:
    case ui::EventType::kMousePressed:
      if (!draggable_area_.Contains(kb_offset.x(), kb_offset.y())) {
        drag_descriptor_.reset();
      } else if (type == ui::EventType::kMousePressed &&
                 !static_cast<const ui::MouseEvent&>(event)
                      .IsOnlyLeftMouseButton()) {
        // Mouse events are limited to just the left mouse button.
        drag_descriptor_.reset();
      } else if (!drag_descriptor_) {
        drag_descriptor_ = std::make_unique<DragDescriptor>(DragDescriptor{
            keyboard_bounds_in_screen.origin(), kb_offset, pointer_id});
      }
      break;

    case ui::EventType::kMouseDragged:
    case ui::EventType::kTouchMoved:
      if (drag_descriptor_ && drag_descriptor_->pointer_id == pointer_id) {
        // Drag continues.
        // If there is an active drag, use it to determine the new location
        // of the keyboard.
        const gfx::Point original_click_location =
            drag_descriptor_->original_keyboard_location +
            drag_descriptor_->original_click_offset;
        const gfx::Point current_drag_location =
            keyboard_bounds_in_screen.origin() + kb_offset;
        const gfx::Vector2d cumulative_drag_offset =
            current_drag_location - original_click_location;
        const gfx::Point new_keyboard_location =
            drag_descriptor_->original_keyboard_location +
            cumulative_drag_offset;
        gfx::Rect new_bounds_in_local =
            gfx::Rect(new_keyboard_location, keyboard_bounds_in_screen.size());

        DisplayUtil display_util;
        const display::Display& new_display =
            display_util.FindAdjacentDisplayIfPointIsNearMargin(
                current_display, current_drag_location);

        if (current_display.id() == new_display.id()) {
          delegate_->MoveKeyboardWindow(new_bounds_in_local);
        } else {
          // Since the keyboard has jumped across screens, cancel the current
          // drag descriptor as though the user has lifted their finger.
          drag_descriptor_.reset();

          gfx::Rect new_bounds_in_screen =
              new_bounds_in_local +
              current_display.bounds().origin().OffsetFromOrigin();
          gfx::Rect contained_new_bounds_in_screen =
              ContainKeyboardToDisplayBounds(new_bounds_in_screen,
                                             new_display.bounds());

          // Enqueue a transition to the adjacent display.
          new_bounds_in_local =
              contained_new_bounds_in_screen -
              new_display.bounds().origin().OffsetFromOrigin();
          delegate_->MoveKeyboardWindowToDisplay(new_display,
                                                 new_bounds_in_local);
        }
        SavePosition(delegate_->GetBoundsInScreen(), new_display.size());
        return true;
      }
      break;

    default:
      drag_descriptor_.reset();
      break;
  }
  return false;
}

bool ContainerFloatingBehavior::HandleGestureEvent(
    const ui::GestureEvent& event,
    const gfx::Rect& bounds_in_screen) {
  return false;
}

void ContainerFloatingBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  gfx::Point keyboard_location =
      GetPositionForShowingKeyboard(container->bounds().size(), display_bounds);
  gfx::Rect keyboard_bounds_in_screen =
      gfx::Rect(keyboard_location, container->bounds().size());
  SavePosition(keyboard_bounds_in_screen, display_bounds.size());
  container->SetBounds(keyboard_bounds_in_screen);
}

bool ContainerFloatingBehavior::TextBlurHidesKeyboard() const {
  return true;
}

gfx::Rect ContainerFloatingBehavior::GetOccludedBounds(
    const gfx::Rect& visual_bounds_in_screen) const {
  return {};
}

bool ContainerFloatingBehavior::OccludedBoundsAffectWorkspaceLayout() const {
  return false;
}

void ContainerFloatingBehavior::SetDraggableArea(const gfx::Rect& rect) {
  draggable_area_ = rect;
}

void ContainerFloatingBehavior::SetAreaToRemainOnScreen(const gfx::Rect& rect) {
  area_in_window_to_remain_on_screen_ = rect;
}

}  //  namespace keyboard
