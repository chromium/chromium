// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

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

bool IsExternalMouseOrTrackpadConnected() {
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

int GetInternalTrackpadDeviceId() {
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

bool IsFromInternalTrackpad(const ui::Event& event) {
  return event.source_device_id() == GetInternalTrackpadDeviceId();
}

constexpr base::TimeDelta kEnableTrackpadKeyPressWindow = base::Seconds(3);
}  // namespace

DisableTrackpadEventRewriter::DisableTrackpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTrackpadEventRewriter(
      this);
}

DisableTrackpadEventRewriter::~DisableTrackpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTrackpadEventRewriter(
      nullptr);
}

void DisableTrackpadEventRewriter::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool DisableTrackpadEventRewriter::IsEnabled() {
  return enabled_;
}

ui::EventDispatchDetails DisableTrackpadEventRewriter::RewriteEvent(
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

ui::EventDispatchDetails DisableTrackpadEventRewriter::HandleMouseOrScrollEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DisableTrackpadMode disable_trackpad_mode =
      Shell::Get()->accessibility_controller()->GetDisableTrackpadMode();
  bool is_internal_trackpad_event = IsFromInternalTrackpad(event);
  bool is_external_device_connected = IsExternalMouseOrTrackpadConnected();

  switch (disable_trackpad_mode) {
    case DisableTrackpadMode::kNever:
      Shell::Get()->cursor_manager()->ShowCursor();
      return SendEvent(continuation, &event);

    case DisableTrackpadMode::kAlways:
      if (is_internal_trackpad_event) {
        Shell::Get()->cursor_manager()->HideCursor();
        return DiscardEvent(continuation);
      }
      return SendEvent(continuation, &event);

    case DisableTrackpadMode::kOnExternalMouseConnected:
      if (is_internal_trackpad_event && is_external_device_connected) {
        Shell::Get()->cursor_manager()->HideCursor();
        return DiscardEvent(continuation);
      }
      return SendEvent(continuation, &event);
  }
}

void DisableTrackpadEventRewriter::HandleKeyEvent(const ui::KeyEvent* event) {
  // TODO(b/365813554): Prevent escape key from propagating to the system before
  // a specified time window between escape key presses.
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }
  event->key_code() == ui::VKEY_ESCAPE ? HandleEscapeKeyPress()
                                       : ResetEscapeKeyPressTracking();
}

void DisableTrackpadEventRewriter::HandleEscapeKeyPress() {
  if (escape_press_count_ == 0) {
    first_escape_press_time_ = ui::EventTimeForNow();
  }

  ++escape_press_count_;
  base::TimeDelta elapsed_time =
      ui::EventTimeForNow() - first_escape_press_time_;

  if (elapsed_time > kEnableTrackpadKeyPressWindow) {
    ResetEscapeKeyPressTracking();
    return;
  }

  if (escape_press_count_ >= 5) {
    SetEnabled(false);
    Shell::Get()->accessibility_controller()->EnableInternalTrackpad();
    ResetEscapeKeyPressTracking();
  }
}

void DisableTrackpadEventRewriter::ResetEscapeKeyPressTracking() {
  escape_press_count_ = 0;
  first_escape_press_time_ = base::TimeTicks();
}

}  // namespace ash
