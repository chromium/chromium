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

MouseKeysController::MouseKeysController() {}

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

  // TODO(259372916): Use a timer instead of relying on key repeats.
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
        MoveMouse(-1, -1);
        return true;

      case ui::DomCode::DIGIT8:
        MoveMouse(0, -1);
        return true;

      case ui::DomCode::DIGIT9:
        MoveMouse(1, -1);
        return true;

      case ui::DomCode::US_U:
        MoveMouse(-1, 0);
        return true;

      case ui::DomCode::US_O:
        MoveMouse(1, 0);
        return true;

      case ui::DomCode::US_J:
        MoveMouse(-1, 1);
        return true;

      case ui::DomCode::US_K:
        MoveMouse(0, 1);
        return true;

      case ui::DomCode::US_L:
        MoveMouse(1, 1);
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
      case ui::DomCode::DIGIT8:
      case ui::DomCode::DIGIT9:
      case ui::DomCode::US_U:
      case ui::DomCode::US_O:
      case ui::DomCode::US_J:
      case ui::DomCode::US_K:
      case ui::DomCode::US_L:
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

void MouseKeysController::MoveMouse(float x_direction, float y_direction) {
  gfx::Point location(
      last_mouse_position_dips_ +
      gfx::Vector2d(x_direction * kMoveDeltaDIP, y_direction * kMoveDeltaDIP));

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

}  // namespace ash
