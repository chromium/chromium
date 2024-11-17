// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_touchpad_event_rewriter.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

bool IsExternalDevice(const ui::InputDevice& device) {
  return device.type == ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH ||
         device.type == ui::InputDeviceType::INPUT_DEVICE_USB;
}

bool IsExternalMouseOrTouchpadConnected() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  InputDeviceSettingsControllerImpl* input_device_settings_controller =
      Shell::Get()->input_device_settings_controller();

  // Check for external touchpads.
  for (const auto& touchpad : device_data_manager->GetTouchpadDevices()) {
    const mojom::Touchpad* found_device =
        input_device_settings_controller->GetTouchpad(touchpad.id);

    if (found_device != nullptr && IsExternalDevice(touchpad)) {
      return true;
    }
  }

  // Check for external mice.
  for (const auto& mouse : device_data_manager->GetMouseDevices()) {
    const mojom::Mouse* found_device =
        input_device_settings_controller->GetMouse(mouse.id);

    if (found_device != nullptr && IsExternalDevice(mouse)) {
      return true;
    }
  }

  return false;
}

int GetInternalTouchpadDeviceId() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  InputDeviceSettingsControllerImpl* input_device_settings_controller =
      Shell::Get()->input_device_settings_controller();

  for (const auto& touchpad : device_data_manager->GetTouchpadDevices()) {
    const mojom::Touchpad* found_device =
        input_device_settings_controller->GetTouchpad(touchpad.id);

    if (found_device != nullptr &&
        touchpad.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return touchpad.id;
    }
  }

  return ui::InputDevice::kInvalidId;
}

bool IsFromInternalTouchpad(const ui::Event& event) {
  return event.source_device_id() == GetInternalTouchpadDeviceId();
}

constexpr base::TimeDelta kEnableTouchpadKeyPressWindow = base::Seconds(2);
}  // namespace

DisableTouchpadEventRewriter::DisableTouchpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTouchpadEventRewriter(
      this);
}

DisableTouchpadEventRewriter::~DisableTouchpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTouchpadEventRewriter(
      nullptr);
}

void DisableTouchpadEventRewriter::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool DisableTouchpadEventRewriter::IsEnabled() {
  return enabled_;
}

ui::EventDispatchDetails DisableTouchpadEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!IsEnabled()) {
    return SendEvent(continuation, &event);
  }

  if (event.IsKeyEvent()) {
    HandleKeyEvent(event.AsKeyEvent());
  }

  if (event.IsMouseEvent() || event.IsScrollEvent()) {
    return HandleMouseOrScrollEvent(event, continuation);
  }

  return SendEvent(continuation, &event);
}

ui::EventDispatchDetails DisableTouchpadEventRewriter::HandleMouseOrScrollEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DisableTouchpadMode disable_touchpad_mode =
      Shell::Get()->accessibility_controller()->GetDisableTouchpadMode();
  bool is_internal_touchpad_event = IsFromInternalTouchpad(event);
  bool is_external_device_connected = IsExternalMouseOrTouchpadConnected();

  switch (disable_touchpad_mode) {
    case DisableTouchpadMode::kNever:
      Shell::Get()->cursor_manager()->ShowCursor();
      return SendEvent(continuation, &event);

    case DisableTouchpadMode::kAlways:
      if (is_internal_touchpad_event) {
        Shell::Get()->cursor_manager()->HideCursor();
        return DiscardEvent(continuation);
      }
      return SendEvent(continuation, &event);

    case DisableTouchpadMode::kOnExternalMouseConnected:
      if (is_internal_touchpad_event && is_external_device_connected) {
        Shell::Get()->cursor_manager()->HideCursor();
        return DiscardEvent(continuation);
      }
      return SendEvent(continuation, &event);
  }
}

void DisableTouchpadEventRewriter::HandleKeyEvent(const ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }
  event->key_code() == ui::VKEY_SHIFT ? HandleShiftKeyPress()
                                      : ResetShiftKeyPressTracking();
}

void DisableTouchpadEventRewriter::HandleShiftKeyPress() {
  if (shift_press_count_ == 0) {
    first_shift_press_time_ = ui::EventTimeForNow();
  }

  ++shift_press_count_;
  base::TimeDelta elapsed_time =
      ui::EventTimeForNow() - first_shift_press_time_;

  if (elapsed_time > kEnableTouchpadKeyPressWindow) {
    ResetShiftKeyPressTracking();
    return;
  }

  if (shift_press_count_ >= 5) {
    SetEnabled(false);
    Shell::Get()->accessibility_controller()->EnableInternalTouchpad();
    ResetShiftKeyPressTracking();
  }
}

void DisableTouchpadEventRewriter::ResetShiftKeyPressTracking() {
  shift_press_count_ = 0;
  first_shift_press_time_ = base::TimeTicks();
}

}  // namespace ash
