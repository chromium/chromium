// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_event_filter.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/display/screen_ash.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_cycle_list.h"
#include "base/bind.h"
#include "ui/events/event.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

// The distance a user has to move their mouse from |initial_mouse_location_|
// before this stops filtering mouse events.
constexpr int kMouseMovementThreshold = 5;

WindowCycleEventFilter::WindowCycleEventFilter()
    : initial_mouse_location_(
          display::Screen::GetScreen()->GetCursorScreenPoint()) {
  Shell::Get()->AddPreTargetHandler(this);
  // Handling release of "Alt" must come before other pretarget handlers
  // (specifically, the partial screenshot handler). See crbug.com/651939
  // We can't do all key event handling that early though because it prevents
  // other accelerators (like triggering a partial screenshot) from working.
  Shell::Get()->AddPreTargetHandler(&alt_release_handler_,
                                    ui::EventTarget::Priority::kSystem);
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->RemovePreTargetHandler(&alt_release_handler_);
}

void WindowCycleEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Until the alt key is released, all key events except the trigger key press
  // (which is handled by the accelerator controller to call Step) are handled
  // by this window cycle controller: https://crbug.com/340339. When the window
  // cycle list exists, right + left arrow keys are considered trigger keys and
  // those two are handled by this.
  const bool is_trigger_key = IsTriggerKey(event);
  const bool is_exit_key = IsExitKey(event);

  if (!is_trigger_key || event->type() != ui::ET_KEY_PRESSED)
    event->StopPropagation();

  if (is_trigger_key)
    HandleTriggerKey(event);
  else if (is_exit_key)
    Shell::Get()->window_cycle_controller()->CompleteCycling();
  else if (event->key_code() == ui::VKEY_ESCAPE)
    Shell::Get()->window_cycle_controller()->CancelCycling();
}

void WindowCycleEventFilter::HandleTriggerKey(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_RELEASED) {
    repeat_timer_.Stop();
  } else if (ShouldRepeatKey(event)) {
    repeat_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(180),
        base::BindRepeating(
            &WindowCycleController::HandleCycleWindow,
            base::Unretained(Shell::Get()->window_cycle_controller()),
            GetDirection(event)));
  } else if (event->key_code() == ui::VKEY_LEFT ||
             event->key_code() == ui::VKEY_RIGHT) {
    Shell::Get()->window_cycle_controller()->HandleCycleWindow(
        GetDirection(event));
  }
}

bool WindowCycleEventFilter::IsTriggerKey(ui::KeyEvent* event) const {
  const bool interactive_trigger_key =
      features::IsInteractiveWindowCycleListEnabled() &&
      (event->key_code() == ui::VKEY_LEFT ||
       event->key_code() == ui::VKEY_RIGHT);

  return event->key_code() == ui::VKEY_TAB ||
         (debug::DeveloperAcceleratorsEnabled() &&
          event->key_code() == ui::VKEY_W) ||
         interactive_trigger_key;
}

bool WindowCycleEventFilter::IsExitKey(ui::KeyEvent* event) const {
  return features::IsInteractiveWindowCycleListEnabled() &&
         (event->key_code() == ui::VKEY_RETURN ||
          event->key_code() == ui::VKEY_SPACE);
}

bool WindowCycleEventFilter::ShouldRepeatKey(ui::KeyEvent* event) const {
  return event->type() == ui::ET_KEY_PRESSED && event->is_repeat() &&
         !repeat_timer_.IsRunning();
}

void WindowCycleEventFilter::SetHasUserUsedMouse(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_ENTERED &&
      event->type() != ui::ET_MOUSE_EXITED) {
    // If a user clicks/drags/scrolls mouse wheel, then they have used the
    // mouse.
    has_user_used_mouse_ = true;
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &event_screen_point);
  if ((initial_mouse_location_ - event_screen_point).Length() >
      kMouseMovementThreshold) {
    has_user_used_mouse_ = true;
  }
}

WindowCycleController::Direction WindowCycleEventFilter::GetDirection(
    ui::KeyEvent* event) const {
  DCHECK(IsTriggerKey(event));

  // Move backward if left arrow, forward if right arrow, tab, or W. Shift flips
  // the direction.
  const bool left = event->key_code() == ui::VKEY_LEFT;
  const bool shift = event->IsShiftDown();

  return (left ^ shift) ? WindowCycleController::BACKWARD
                        : WindowCycleController::FORWARD;
}

void WindowCycleEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (!has_user_used_mouse_)
    SetHasUserUsedMouse(event);

  if (features::IsInteractiveWindowCycleListEnabled()) {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    const bool cycle_list_is_visible =
        window_cycle_controller->IsWindowListVisible();
    const bool event_should_not_be_filtered =
        has_user_used_mouse_ &&
        window_cycle_controller->IsEventInCycleView(event);
    if (event_should_not_be_filtered || !cycle_list_is_visible) {
      return;
    } else if (event->type() == ui::ET_MOUSE_PRESSED && cycle_list_is_visible) {
      // Close the window cycle list if a user clicks outside of it.
      window_cycle_controller->CancelCycling();
    }
  }

  // Prevent mouse clicks from doing anything while the Alt+Tab UI is active
  // <crbug.com/641171> but don't interfere with drag and drop operations
  // <crbug.com/660945>.
  if (event->type() != ui::ET_MOUSE_DRAGGED &&
      event->type() != ui::ET_MOUSE_RELEASED) {
    event->StopPropagation();
  }
}

void WindowCycleEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (features::IsInteractiveWindowCycleListEnabled() &&
      Shell::Get()->window_cycle_controller()->IsEventInCycleView(event)) {
    return;
  }

  // Prevent any form of tap from doing anything while the Alt+Tab UI is active.
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_DOUBLE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_CANCEL ||
      event->type() == ui::ET_GESTURE_TAP_DOWN ||
      event->type() == ui::ET_GESTURE_TAP_UNCONFIRMED ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS ||
      event->type() == ui::ET_GESTURE_LONG_TAP) {
    event->StopPropagation();
  }
}

WindowCycleEventFilter::AltReleaseHandler::AltReleaseHandler() = default;

WindowCycleEventFilter::AltReleaseHandler::~AltReleaseHandler() = default;

void WindowCycleEventFilter::AltReleaseHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace ash
