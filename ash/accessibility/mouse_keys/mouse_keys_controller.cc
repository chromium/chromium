// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"

#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

MouseKeysController::MouseKeysController() {
  SetMaxSpeed(kDefaultMaxSpeed);
  for (int c = 0; c < kKeyCount; ++c) {
    pressed_keys_[c] = false;
  }
}

MouseKeysController::~MouseKeysController() {
  // Disable to ensure we've removed our event handlers from Shell.
  SetEnabled(false);
}

bool MouseKeysController::RewriteEvent(const ui::Event& event) {
  if (!event.IsKeyEvent()) {
    return false;
  }

  int modifier_mask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                      ui::EF_IS_EXTENDED_KEY;
  event_flags_ = event.flags() & modifier_mask;

  // TODO(259372916): Use an accelerator instead of hard coding this.
  // TODO(259372916): Add a pref to remember the enabled state.
  const ui::KeyEvent* key_event = event.AsKeyEvent();
  if (key_event->type() == ui::ET_KEY_PRESSED &&
      key_event->code() == ui::DomCode::US_M &&
      key_event->flags() & ui::EF_CONTROL_DOWN &&
      key_event->flags() & ui::EF_SHIFT_DOWN &&
      !(key_event->flags() & ui::EF_IS_REPEAT)) {
    SetEnabled(!enabled_);
    return true;
  }

  if (!enabled_) {
    return false;
  }

  CenterMouseIfUninitialized();

  if (key_event->type() == ui::ET_KEY_PRESSED) {
    switch (key_event->code()) {
      case ui::DomCode::US_I:
        // Ignore key repeat to avoid multiple clicks.
        if (!(key_event->flags() & ui::EF_IS_REPEAT)) {
          SendMouseEventToLocation(ui::ET_MOUSE_PRESSED,
                                   last_mouse_position_dips_);
        }
        return true;

      case ui::DomCode::DIGIT7:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyUpLeft);
        return true;

      case ui::DomCode::DIGIT8:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyUp);
        return true;

      case ui::DomCode::DIGIT9:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyUpRight);
        return true;

      case ui::DomCode::US_U:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyLeft);
        return true;

      case ui::DomCode::US_O:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyRight);
        return true;

      case ui::DomCode::US_J:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyDownLeft);
        return true;

      case ui::DomCode::US_K:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyDown);
        return true;

      case ui::DomCode::US_L:
        CheckFlagsAndMaybePressKey(key_event->flags(), kKeyDownRight);
        return true;

      default:
        break;
    }
  } else {
    switch (key_event->code()) {
      case ui::DomCode::US_I:
        // Release the mouse on key up.
        if (key_event->type() == ui::ET_KEY_RELEASED) {
          SendMouseEventToLocation(ui::ET_MOUSE_RELEASED,
                                   last_mouse_position_dips_);
        }
        return true;

        // Ignore other key events from bound keys.
      case ui::DomCode::DIGIT7:
        ReleaseKey(kKeyUpLeft);
        return true;

      case ui::DomCode::DIGIT8:
        ReleaseKey(kKeyUp);
        return true;

      case ui::DomCode::DIGIT9:
        ReleaseKey(kKeyUpRight);
        return true;

      case ui::DomCode::US_U:
        ReleaseKey(kKeyLeft);
        return true;

      case ui::DomCode::US_O:
        ReleaseKey(kKeyRight);
        return true;

      case ui::DomCode::US_J:
        ReleaseKey(kKeyDownLeft);
        return true;

      case ui::DomCode::US_K:
        ReleaseKey(kKeyDown);
        return true;

      case ui::DomCode::US_L:
        ReleaseKey(kKeyDownRight);
        return true;

      default:
        break;
    }
  }

  return false;
}

void MouseKeysController::SetEnabled(bool enabled) {
  if (enabled && !enabled_) {
    Shell::Get()->AddAccessibilityEventHandler(
        this, AccessibilityEventHandlerManager::HandlerType::kMouseKeys);
  } else if (!enabled && enabled_) {
    Shell::Get()->RemoveAccessibilityEventHandler(this);
  }
  enabled_ = enabled;
}

void MouseKeysController::OnMouseEvent(ui::MouseEvent* event) {
  bool is_synthesized = event->IsSynthesized() ||
                        event->source_device_id() == ui::ED_UNKNOWN_DEVICE;
  if (is_synthesized || event->type() != ui::ET_MOUSE_MOVED) {
    return;
  }
  if (event->target()) {
    last_mouse_position_dips_ = event->target()->GetScreenLocation(*event);
  }
}

void MouseKeysController::SendMouseEventToLocation(ui::EventType type,
                                                   const gfx::Point& location) {
  const int button = ui::EF_LEFT_MOUSE_BUTTON;
  aura::Window* root_window = window_util::GetRootWindowAt(location);
  DCHECK(root_window)
      << "Root window not found while attempting mouse keys click.";
  gfx::Point location_in_pixels(location);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);
  ui::MouseEvent press_event(type, location_in_pixels, location_in_pixels,
                             ui::EventTimeForNow(), event_flags_ | button,
                             button);

  (void)host->GetEventSink()->OnEventFromSource(&press_event);
}

void MouseKeysController::MoveMouse(const gfx::Vector2d& move_delta_dip) {
  gfx::Point location = last_mouse_position_dips_ + move_delta_dip;

  // Update the cursor position; this will generate a synthetic mouse event that
  // will pass through the standard event flow.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(location);
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  if (!host) {
    return;
  }

  // Show the cursor if needed.
  auto* cursor_client = aura::client::GetCursorClient(host->window());
  if (cursor_client && !cursor_client->IsCursorVisible()) {
    cursor_client->ShowCursor();
  }

  host->MoveCursorToLocationInDIP(location);
  last_mouse_position_dips_ = location;
}

void MouseKeysController::CenterMouseIfUninitialized() {
  if (last_mouse_position_dips_ == gfx::Point(-1, -1)) {
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    DCHECK(root_window)
        << "Root window not found while attempting to center mouse.";
    last_mouse_position_dips_ = root_window->bounds().CenterPoint();
  }
}

void MouseKeysController::CheckFlagsAndMaybePressKey(int flags, MouseKey key) {
  if (!(flags & ui::EF_IS_REPEAT)) {
    PressKey(key);
  }
}

void MouseKeysController::PressKey(MouseKey key) {
  pressed_keys_[key] = true;
  RefreshVelocity();
}

void MouseKeysController::ReleaseKey(MouseKey key) {
  pressed_keys_[key] = false;
  RefreshVelocity();
}

void MouseKeysController::RefreshVelocity() {
  int x_direction = 0;
  int y_direction = 0;

  if (pressed_keys_[kKeyUpLeft] || pressed_keys_[kKeyLeft] ||
      pressed_keys_[kKeyDownLeft]) {
    // Left takes precedence.
    x_direction = -1;
  } else if (pressed_keys_[kKeyUpRight] || pressed_keys_[kKeyRight] ||
             pressed_keys_[kKeyDownRight]) {
    x_direction = 1;
  }

  if (pressed_keys_[kKeyUpLeft] || pressed_keys_[kKeyUp] ||
      pressed_keys_[kKeyUpRight]) {
    // Up takes precedence.
    y_direction = -1;
  } else if (pressed_keys_[kKeyDownLeft] || pressed_keys_[kKeyDown] ||
             pressed_keys_[kKeyDownRight]) {
    y_direction = 1;
  }

  // Set the base movement.
  move_direction_ = gfx::Vector2d(x_direction, y_direction);

  if (x_direction == 0 && y_direction == 0) {
    // Reset everything if there is no movement.
    speed_ = 0;
    if (update_timer_.IsRunning()) {
      update_timer_.Stop();
    }
    return;
  }

  if (speed_ == 0) {
    // If movement is just starting, initialize everything.
    if (acceleration_ == 0) {
      // If there is no acceleration, start at the max speed.
      speed_ = max_speed_;
    } else {
      speed_ = kBaseSpeedDIPPerSecond * kUpdateFrequencyInSeconds;
    }
    update_timer_.Start(FROM_HERE, base::Seconds(kUpdateFrequencyInSeconds),
                        this, &MouseKeysController::UpdateState);
  }

  UpdateState();
}

void MouseKeysController::UpdateState() {
  MoveMouse(gfx::Vector2d(move_direction_.x() * speed_,
                          move_direction_.y() * speed_));
  double acceleration = acceleration_ * kBaseAccelerationDIPPerSecondSquared *
                        kUpdateFrequencyInSeconds;
  speed_ = std::clamp(speed_ + acceleration, 0.0, max_speed_);
}

}  // namespace ash
