// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"

#include <array>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/drag_event_rewriter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_controller.h"
#include "ash/wm/window_util.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const base::flat_map<ui::DomCode, MouseKeysController::MouseKey>
    kLeftHandedKeys({
        {ui::DomCode::US_W, MouseKeysController::kKeyClick},
        {ui::DomCode::US_V, MouseKeysController::kKeyDoubleClick},
        {ui::DomCode::US_Z, MouseKeysController::kKeyDragStart},
        {ui::DomCode::US_C, MouseKeysController::kKeyDragStop},
        {ui::DomCode::DIGIT1, MouseKeysController::kKeyUpLeft},
        {ui::DomCode::DIGIT2, MouseKeysController::kKeyUp},
        {ui::DomCode::DIGIT3, MouseKeysController::kKeyUpRight},
        {ui::DomCode::US_Q, MouseKeysController::kKeyLeft},
        {ui::DomCode::US_E, MouseKeysController::kKeyRight},
        {ui::DomCode::US_A, MouseKeysController::kKeyDownLeft},
        {ui::DomCode::US_S, MouseKeysController::kKeyDown},
        {ui::DomCode::US_D, MouseKeysController::kKeyDownRight},
        {ui::DomCode::US_X, MouseKeysController::kKeySelectNextButton},
    });

const base::flat_map<ui::DomCode, MouseKeysController::MouseKey>
    kRightHandedKeys({
        {ui::DomCode::US_I, MouseKeysController::kKeyClick},
        {ui::DomCode::SLASH, MouseKeysController::kKeyDoubleClick},
        {ui::DomCode::US_M, MouseKeysController::kKeyDragStart},
        {ui::DomCode::PERIOD, MouseKeysController::kKeyDragStop},
        {ui::DomCode::DIGIT7, MouseKeysController::kKeyUpLeft},
        {ui::DomCode::DIGIT8, MouseKeysController::kKeyUp},
        {ui::DomCode::DIGIT9, MouseKeysController::kKeyUpRight},
        {ui::DomCode::US_U, MouseKeysController::kKeyLeft},
        {ui::DomCode::US_O, MouseKeysController::kKeyRight},
        {ui::DomCode::US_J, MouseKeysController::kKeyDownLeft},
        {ui::DomCode::US_K, MouseKeysController::kKeyDown},
        {ui::DomCode::US_L, MouseKeysController::kKeyDownRight},
        {ui::DomCode::COMMA, MouseKeysController::kKeySelectNextButton},
    });

const base::flat_map<ui::DomCode, MouseKeysController::MouseKey> kNumPadKeys({
    {ui::DomCode::NUMPAD5, MouseKeysController::kKeyClick},
    {ui::DomCode::NUMPAD_ADD, MouseKeysController::kKeyDoubleClick},
    {ui::DomCode::NUMPAD0, MouseKeysController::kKeyDragStart},
    {ui::DomCode::NUMPAD_DECIMAL, MouseKeysController::kKeyDragStop},
    {ui::DomCode::NUMPAD7, MouseKeysController::kKeyUpLeft},
    {ui::DomCode::NUMPAD8, MouseKeysController::kKeyUp},
    {ui::DomCode::NUMPAD9, MouseKeysController::kKeyUpRight},
    {ui::DomCode::NUMPAD4, MouseKeysController::kKeyLeft},
    {ui::DomCode::NUMPAD6, MouseKeysController::kKeyRight},
    {ui::DomCode::NUMPAD1, MouseKeysController::kKeyDownLeft},
    {ui::DomCode::NUMPAD2, MouseKeysController::kKeyDown},
    {ui::DomCode::NUMPAD3, MouseKeysController::kKeyDownRight},
    {ui::DomCode::NUMPAD_DIVIDE, MouseKeysController::kKeySelectLeftButton},
    {ui::DomCode::NUMPAD_SUBTRACT, MouseKeysController::kKeySelectRightButton},
    {ui::DomCode::NUMPAD_MULTIPLY, MouseKeysController::kKeySelectBothButtons},
});

bool ShouldEndDragOperation(ui::MouseEvent* event) {
  return event->type() == ui::EventType::kMousePressed && event->IsAnyButton();
}

}  // namespace

MouseKeysController::MouseKeysController()
    : drag_event_rewriter_(std::make_unique<DragEventRewriter>()) {
  SetMaxSpeed(kDefaultMaxSpeed);
  pressed_keys_.fill(false);
  Shell::Get()->AddAccessibilityEventHandler(
      this, AccessibilityEventHandlerManager::HandlerType::kMouseKeys);
  mouse_keys_bubble_controller_ = std::make_unique<MouseKeysBubbleController>();
  Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource()->AddEventRewriter(
      drag_event_rewriter_.get());
}

MouseKeysController::~MouseKeysController() {
  Shell* shell = Shell::Get();
  shell->RemoveAccessibilityEventHandler(this);

  auto* root_window = Shell::GetPrimaryRootWindow();
  if (!root_window) {
    return;
  }

  auto* host = root_window->GetHost();
  if (!host) {
    return;
  }

  auto* event_source = host->GetEventSource();
  if (!event_source) {
    return;
  }

  event_source->RemoveEventRewriter(drag_event_rewriter_.get());
}

void MouseKeysController::Toggle() {
  paused_ = !paused_;
  if (paused_) {
    UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kHidden,
                          IDS_ASH_MOUSE_KEYS_PAUSED);
    // Reset everything when pausing.
    ResetMovement();
    dragging_ = false;
  } else {
    UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kHidden,
                          IDS_ASH_MOUSE_KEYS_RESUMED);
  }
}

bool MouseKeysController::RewriteEvent(const ui::Event& event) {
  if (!enabled_ || !event.IsKeyEvent()) {
    return false;
  }

  int modifier_mask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                      ui::EF_IS_EXTENDED_KEY;
  event_flags_ = event.flags() & modifier_mask;

  if (paused_) {
    return false;
  }

  CenterMouseIfUninitialized();

  // Check primary keyboard keys.
  const ui::KeyEvent* key_event = event.AsKeyEvent();
  if (use_primary_keys_) {
    auto mappings = left_handed_ ? kLeftHandedKeys : kRightHandedKeys;
    for (auto mapping : mappings) {
      if (CheckFlagsAndMaybeSendEvent(*key_event, mapping.first,
                                      mapping.second)) {
        return true;
      }
    }
  }

  // Check num pad.
  for (auto mapping : kNumPadKeys) {
    if (CheckFlagsAndMaybeSendEvent(*key_event, mapping.first,
                                    mapping.second)) {
      return true;
    }
  }

  return false;
}

void MouseKeysController::set_enabled(bool enabled) {
  if (enabled == enabled_) {
    return;
  }

  enabled_ = enabled;
  paused_ = false;
}

void MouseKeysController::OnMouseEvent(ui::MouseEvent* event) {
  if (ShouldEndDragOperation(event)) {
    EndDragOperation();
    return;
  }

  if (event->type() != ui::EventType::kMouseMoved) {
    return;
  }

  if (event->target()) {
    last_mouse_position_dips_ = event->target()->GetScreenLocation(*event);

    gfx::Point bubble_position = last_mouse_position_dips_;
    bubble_position.Offset(16, 16);
    mouse_keys_bubble_controller_->UpdateMouseKeysBubblePosition(
        bubble_position);
  }
}

void MouseKeysController::EndDragOperation() {
  if (dragging_) {
    drag_event_rewriter_->SetEnabled(false);
    SendMouseEventToLocation(ui::EventType::kMouseReleased,
                             last_mouse_position_dips_);
    dragging_ = false;
    UpdateMouseKeysBubble(false, MouseKeysBubbleIconType::kMouseDrag,
                          IDS_ASH_MOUSE_KEYS_PERIOD_RELEASE);
  }
}

void MouseKeysController::SendMouseEventToLocation(ui::EventType type,
                                                   const gfx::Point& location,
                                                   int flags) {
  int event_flags = event_flags_ | flags;
  int button = 0;
  switch (current_mouse_button_) {
    case kLeft:
      button = ui::EF_LEFT_MOUSE_BUTTON;
      break;
    case kRight:
      button = ui::EF_RIGHT_MOUSE_BUTTON;
      break;
    case kBoth:
      button = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON;
      break;
  }
  aura::Window* root_window = window_util::GetRootWindowAt(location);
  DCHECK(root_window)
      << "Root window not found while attempting mouse keys click.";
  gfx::Point location_in_pixels(location);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);
  ui::MouseEvent event(type, location_in_pixels, location_in_pixels,
                       ui::EventTimeForNow(), event_flags | button, button);

  (void)host->GetEventSink()->OnEventFromSource(&event);
}

void MouseKeysController::MoveMouse(const gfx::Vector2d& move_delta_dip) {
  gfx::Point location_in_screen = last_mouse_position_dips_ + move_delta_dip;

  const display::Display& target_display =
      display::Screen::Get()->GetDisplayNearestPoint(location_in_screen);
  auto* host = ash::GetWindowTreeHostForDisplay(target_display.id());
  if (!host) {
    return;
  }

  aura::Window* root_window = host->window();
  DCHECK(root_window)
      << "Root window not found while attempting mouse keys move.";

  // Show the cursor if needed.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client && !cursor_client->IsCursorVisible()) {
    cursor_client->ShowCursor();
  }

  gfx::Point location_in_host = location_in_screen;
  ::wm::ConvertPointFromScreen(root_window, &location_in_host);

  host->MoveCursorToLocationInDIP(location_in_host);
  if (dragging_) {
    SendMouseEventToLocation(ui::EventType::kMouseDragged, location_in_screen);
  }
  last_mouse_position_dips_ = location_in_screen;
}

void MouseKeysController::CenterMouseIfUninitialized() {
  if (last_mouse_position_dips_ == gfx::Point(-1, -1)) {
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    DCHECK(root_window)
        << "Root window not found while attempting to center mouse.";
    last_mouse_position_dips_ = root_window->bounds().CenterPoint();
  }
}

bool MouseKeysController::CheckFlagsAndMaybeSendEvent(
    const ui::KeyEvent& key_event,
    ui::DomCode input,
    MouseKey output) {
  if (key_event.code() != input) {
    return false;
  }

  // Ignore key repeats but still consume them.
  if (key_event.flags() & ui::EF_IS_REPEAT) {
    return true;
  }

  // All KeyEvents are either EventType::kKeyPressed or EventType::kKeyReleased.
  if (key_event.type() == ui::EventType::kKeyPressed) {
    PressKey(output);
  } else {
    DCHECK_EQ(key_event.type(), ui::EventType::kKeyReleased);
    ReleaseKey(output);
  }
  return true;
}

void MouseKeysController::PressKey(MouseKey key) {
  int drag_resource = left_handed_ ? IDS_ASH_MOUSE_KEYS_C_KEY_RELEASE
                                  : IDS_ASH_MOUSE_KEYS_PERIOD_RELEASE;
  pressed_keys_[key] = true;
  switch (key) {
    case kKeyUpLeft:
    case kKeyUp:
    case kKeyUpRight:
    case kKeyLeft:
    case kKeyRight:
    case kKeyDownLeft:
    case kKeyDown:
    case kKeyDownRight:
      RefreshVelocity();
      break;
    case kKeyClick:
    case kKeyDragStart:
      if (dragging_) {
        break;
      }
      SendMouseEventToLocation(ui::EventType::kMousePressed,
                               last_mouse_position_dips_);
      dragging_ = true;
      if (key == kKeyDragStart) {
        drag_event_rewriter_->SetEnabled(true);
        UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kMouseDrag,
                              drag_resource);
      }
      break;
    case kKeyDragStop:
      EndDragOperation();
      break;
    case kKeyDoubleClick:
      if (current_mouse_button_ == kLeft) {
        SendMouseEventToLocation(ui::EventType::kMousePressed,
                                 last_mouse_position_dips_);
        SendMouseEventToLocation(ui::EventType::kMouseReleased,
                                 last_mouse_position_dips_);
        SendMouseEventToLocation(ui::EventType::kMousePressed,
                                 last_mouse_position_dips_,
                                 ui::EF_IS_DOUBLE_CLICK);
        SendMouseEventToLocation(ui::EventType::kMouseReleased,
                                 last_mouse_position_dips_,
                                 ui::EF_IS_DOUBLE_CLICK);
      }
      break;
    case kKeySelectLeftButton:
      UpdateCurrentMouseButton(kLeft);
      break;
    case kKeySelectRightButton:
      UpdateCurrentMouseButton(kRight);
      break;
    case kKeySelectBothButtons:
      UpdateCurrentMouseButton(kBoth);
      break;
    case kKeySelectNextButton:
      SelectNextButton();
      break;
    case kKeyCount:
      NOTREACHED();
  }
}

void MouseKeysController::ReleaseKey(MouseKey key) {
  pressed_keys_[key] = false;
  switch (key) {
    case kKeyUpLeft:
    case kKeyUp:
    case kKeyUpRight:
    case kKeyLeft:
    case kKeyRight:
    case kKeyDownLeft:
    case kKeyDown:
    case kKeyDownRight:
      RefreshVelocity();
      break;
    case kKeyClick:
      if (dragging_) {
        SendMouseEventToLocation(ui::EventType::kMouseReleased,
                                 last_mouse_position_dips_);
        dragging_ = false;
      }
      break;
    case kKeyDragStart:
    case kKeyDragStop:
    case kKeyDoubleClick:
    case kKeySelectLeftButton:
    case kKeySelectRightButton:
    case kKeySelectBothButtons:
    case kKeySelectNextButton:
      break;
    case kKeyCount:
      NOTREACHED();
  }
}

void MouseKeysController::SelectNextButton() {
  switch (current_mouse_button_) {
    case kLeft:
      UpdateCurrentMouseButton(kRight);
      break;
    case kRight:
      UpdateCurrentMouseButton(kBoth);
      break;
    case kBoth:
      UpdateCurrentMouseButton(kLeft);
      break;
  }
}

void MouseKeysController::UpdateCurrentMouseButton(MouseButton mouse_button) {
  switch (mouse_button) {
    case kLeft:
      current_mouse_button_ = kLeft;
      UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kButtonChanged,
                            IDS_ASH_MOUSE_KEYS_LEFT_MOUSE_BUTTON);
      break;
    case kRight:
      current_mouse_button_ = kRight;
      UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kButtonChanged,
                            IDS_ASH_MOUSE_KEYS_RIGHT_MOUSE_BUTTON);
      break;
    case kBoth:
      current_mouse_button_ = kBoth;
      UpdateMouseKeysBubble(true, MouseKeysBubbleIconType::kButtonChanged,
                            IDS_ASH_MOUSE_KEYS_BOTH_MOUSE_BUTTONS);
      break;
  }
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
    ResetMovement();
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

void MouseKeysController::UpdateMouseKeysBubble(bool visible,
                                                MouseKeysBubbleIconType icon,
                                                const int name_resource_id) {
  mouse_keys_bubble_controller_->UpdateBubble(
      visible, icon, l10n_util::GetStringUTF16(name_resource_id));
}

void MouseKeysController::ResetMovement() {
  speed_ = 0;
  if (update_timer_.IsRunning()) {
    update_timer_.Stop();
  }
}

MouseKeysBubbleController*
MouseKeysController::GetMouseKeysBubbleControllerForTest() {
  return mouse_keys_bubble_controller_.get();
}

}  // namespace ash
