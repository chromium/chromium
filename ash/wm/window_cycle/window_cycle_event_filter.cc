// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_event_filter.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_ash.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The distance a user has to move their mouse from |initial_mouse_location_|
// before this stops filtering mouse events.
constexpr int kMouseMovementThreshold = 5;

// Is reverse scrolling for mouse wheel on.
bool IsReverseScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kMouseReverseScroll);
}

// Returns whether `event` is a trigger key (tab, left, right, w (when
// debugging)).
bool IsTriggerKey(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  const bool interactive_trigger_key =
      (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT);

  const bool nav_trigger_key =
      Shell::Get()
          ->window_cycle_controller()
          ->IsInteractiveAltTabModeAllowed() &&
      (key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
       key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT);

  return key_code == ui::VKEY_TAB ||
         (debug::DeveloperAcceleratorsEnabled() && key_code == ui::VKEY_W) ||
         interactive_trigger_key || nav_trigger_key;
}

// Returns whether `event` is an exit key (return, space).
bool IsExitKey(ui::KeyEvent* event) {
  return event->key_code() == ui::VKEY_RETURN ||
         event->key_code() == ui::VKEY_SPACE;
}

}  // namespace

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

  if (!is_trigger_key || event->type() != ui::EventType::kKeyPressed) {
    event->StopPropagation();
  }

  if (is_trigger_key)
    HandleTriggerKey(event);
  else if (is_exit_key)
    Shell::Get()->window_cycle_controller()->CompleteCycling();
  else if (event->key_code() == ui::VKEY_ESCAPE)
    Shell::Get()->window_cycle_controller()->CancelCycling();
}

void WindowCycleEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (!has_user_used_mouse_)
    SetHasUserUsedMouse(event);

  if (has_user_used_mouse_) {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    const bool cycle_list_is_visible =
        window_cycle_controller->IsWindowListVisible();
    if (cycle_list_is_visible)
      ProcessMouseEvent(event);

    if (window_cycle_controller->IsEventInCycleView(event) ||
        !cycle_list_is_visible) {
      return;
    }
  }

  // Prevent mouse clicks from doing anything while the Alt+Tab UI is active
  // <crbug.com/641171> but don't interfere with drag and drop operations
  // <crbug.com/660945>.
  if (event->type() != ui::EventType::kMouseDragged &&
      event->type() != ui::EventType::kMouseReleased) {
    event->StopPropagation();
  }
}

void WindowCycleEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  // EventType::kScrollFlingCancel means a touchpad swipe has started.
  if (event->type() == ui::EventType::kScrollFlingCancel) {
    scroll_data_ = ScrollData();
    return;
  }

  // EventType::kScrollFlingStart means a touchpad swipe has ended.
  if (event->type() == ui::EventType::kScrollFlingStart) {
    scroll_data_.reset();
    return;
  }

  DCHECK_EQ(ui::EventType::kScroll, event->type());

  if (ProcessEventImpl(event->finger_count(), event->x_offset(),
                       event->y_offset())) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void WindowCycleEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (Shell::Get()->window_cycle_controller()->IsEventInTabSliderContainer(
          event)) {
    // Return immediately if the event is on the tab slider container. Pass
    // the event to the tab slider buttons to handle it.
    return;
  }
  ProcessGestureEvent(event);
}

void WindowCycleEventFilter::HandleTriggerKey(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  if (event->type() == ui::EventType::kKeyReleased) {
    repeat_timer_.Stop();
  } else if (ShouldRepeatKey(event)) {
    repeat_timer_.Start(
        FROM_HERE, base::Milliseconds(180),
        base::BindRepeating(
            &WindowCycleController::HandleCycleWindow,
            base::Unretained(Shell::Get()->window_cycle_controller()),
            GetWindowCyclingDirection(event), /*same_app_only=*/false));
  } else if (key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
             key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT) {
    Shell::Get()->window_cycle_controller()->HandleKeyboardNavigation(
        GetKeyboardNavDirection(event));
  }
}

bool WindowCycleEventFilter::ShouldRepeatKey(ui::KeyEvent* event) const {
  return event->type() == ui::EventType::kKeyPressed && event->is_repeat() &&
         !repeat_timer_.IsRunning();
}

void WindowCycleEventFilter::SetHasUserUsedMouse(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMouseMoved &&
      event->type() != ui::EventType::kMouseEntered &&
      event->type() != ui::EventType::kMouseExited) {
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

void WindowCycleEventFilter::ProcessMouseEvent(ui::MouseEvent* event) {
  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
  if (event->type() == ui::EventType::kMousePressed &&
      !window_cycle_controller->IsEventInCycleView(event)) {
    // Close the window cycle list if a user clicks outside of it.
    window_cycle_controller->CancelCycling();
    return;
  }

  if (event->IsMouseWheelEvent()) {
    if (!scroll_data_)
      scroll_data_ = ScrollData();
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
    const float y_offset = wheel_event->y_offset();
    // Convert mouse wheel events into three-finger scrolls for window cycle
    // list and also swap y offset with x offset.
    if (ProcessEventImpl(/*finger_count=*/3,
                         IsReverseScrollOn() ? y_offset : -y_offset,
                         wheel_event->x_offset())) {
      event->SetHandled();
      event->StopPropagation();
    }
  }
}

void WindowCycleEventFilter::ProcessGestureEvent(ui::GestureEvent* event) {
  bool should_complete_cycling = false;
  switch (event->type()) {
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapDown:
    case ui::EventType::kGestureDoubleTap:
    case ui::EventType::kGestureTapUnconfirmed:
    case ui::EventType::kGestureTwoFingerTap:
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap: {
      tapped_window_ =
          Shell::Get()->window_cycle_controller()->GetWindowAtPoint(
              event->AsLocatedEvent());
      break;
    }
    case ui::EventType::kGestureTapCancel:
      // Do nothing because the event after this one determines whether we
      // scrolled or tapped.
      break;
    case ui::EventType::kGestureScrollBegin: {
      tapped_window_ = nullptr;
      if (!Shell::Get()->window_cycle_controller()->IsEventInCycleView(event))
        return;

      touch_scrolling_ = true;
      break;
    }
    case ui::EventType::kGestureScrollUpdate: {
      if (!touch_scrolling_)
        return;

      Shell::Get()->window_cycle_controller()->Drag(
          event->details().scroll_x());
      break;
    }
    case ui::EventType::kScrollFlingStart: {
      tapped_window_ = nullptr;
      auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
      if (!window_cycle_controller->IsEventInCycleView(event))
        return;

      // Only start a fling if the x-velocity is non-zero to avoid crashing when
      // creating a fling curve. See crbug.com/1224969.
      float velocity_x = event->details().velocity_x();
      if (velocity_x != 0.f)
        window_cycle_controller->StartFling(velocity_x);
      break;
    }
    case ui::EventType::kGestureEnd: {
      if (tapped_window_) {
        // Defer calling WindowCycleController::CompleteCycling() until we've
        // set |event| to handled and stop its propagation.
        should_complete_cycling = true;
      }
      tapped_window_ = nullptr;
      touch_scrolling_ = false;
      break;
    }
    default:
      if (tapped_window_) {
        Shell::Get()->window_cycle_controller()->SetFocusedWindow(
            tapped_window_);
        break;
      }
      return;
  }

  event->SetHandled();
  event->StopPropagation();

  if (should_complete_cycling)
    Shell::Get()->window_cycle_controller()->CompleteCycling();
}

bool WindowCycleEventFilter::ProcessEventImpl(int finger_count,
                                              float delta_x,
                                              float delta_y) {
  if (!scroll_data_)
    return false;

  if (finger_count != 2 && finger_count != 3) {
    scroll_data_.reset();
    return false;
  }

  if (scroll_data_->finger_count != 0 &&
      scroll_data_->finger_count != finger_count) {
    scroll_data_.reset();
    return false;
  }

  if (finger_count == 2 && !window_util::IsNaturalScrollOn()) {
    // Two finger swipe from left to right should move the list right regardless
    // of natural scroll settings.
    delta_x = -delta_x;
  }

  scroll_data_->scroll_x += delta_x;
  scroll_data_->scroll_y += delta_y;

  const bool moved = CycleWindowCycleList(finger_count, scroll_data_->scroll_x,
                                          scroll_data_->scroll_y);

  if (moved)
    scroll_data_ = ScrollData();
  scroll_data_->finger_count = finger_count;
  return moved;
}

bool WindowCycleEventFilter::CycleWindowCycleList(int finger_count,
                                                  float scroll_x,
                                                  float scroll_y) {
  if (finger_count != 2 && finger_count != 3)
    return false;

  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
  if (!window_cycle_controller->IsCycling() ||
      std::fabs(scroll_x) < std::fabs(scroll_y) ||
      std::fabs(scroll_x) < kHorizontalThresholdDp) {
    return false;
  }

  window_cycle_controller->HandleCycleWindow(
      scroll_x > 0 ? WindowCycleController::WindowCyclingDirection::kForward
                   : WindowCycleController::WindowCyclingDirection::kBackward);
  return true;
}

WindowCycleController::WindowCyclingDirection
WindowCycleEventFilter::GetWindowCyclingDirection(ui::KeyEvent* event) const {
  DCHECK(IsTriggerKey(event));

  // Move backward if left arrow, forward if right arrow, tab, or W. Shift flips
  // the direction.
  const bool left = event->key_code() == ui::VKEY_LEFT;
  const bool shift = event->IsShiftDown();

  return (left ^ shift)
             ? WindowCycleController::WindowCyclingDirection::kBackward
             : WindowCycleController::WindowCyclingDirection::kForward;
}

WindowCycleController::KeyboardNavDirection
WindowCycleEventFilter::GetKeyboardNavDirection(ui::KeyEvent* event) const {
  DCHECK(IsTriggerKey(event));
  switch (event->key_code()) {
    case ui::VKEY_UP:
      return WindowCycleController::KeyboardNavDirection::kUp;
    case ui::VKEY_DOWN:
      return WindowCycleController::KeyboardNavDirection::kDown;
    case ui::VKEY_LEFT:
      return WindowCycleController::KeyboardNavDirection::kLeft;
    case ui::VKEY_RIGHT:
      return WindowCycleController::KeyboardNavDirection::kRight;
    default:
      NOTREACHED();
  }
}

WindowCycleEventFilter::AltReleaseHandler::AltReleaseHandler() = default;

WindowCycleEventFilter::AltReleaseHandler::~AltReleaseHandler() = default;

void WindowCycleEventFilter::AltReleaseHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::EventType::kKeyReleased) {
    event->StopPropagation();
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace ash
