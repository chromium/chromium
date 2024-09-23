// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/touch/touch_devices_controller.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

InternalInputDevicesEventBlocker::InternalInputDevicesEventBlocker() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

InternalInputDevicesEventBlocker::~InternalInputDevicesEventBlocker() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  if (should_be_blocked_)
    UpdateInternalInputDevices(/*should_block=*/false);
}

void InternalInputDevicesEventBlocker::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kKeyboard) {
    UpdateInternalKeyboard(should_be_blocked_);
  }
  if (input_device_types & ui::InputDeviceEventObserver::kTouchpad) {
    UpdateInternalTouchpad(should_be_blocked_);
  }
}

void InternalInputDevicesEventBlocker::UpdateInternalInputDevices(
    bool should_be_blocked) {
  should_be_blocked_ = should_be_blocked;

  UpdateInternalTouchpad(should_be_blocked);
  UpdateInternalKeyboard(should_be_blocked);
}

bool InternalInputDevicesEventBlocker::HasInternalTouchpad() {
  for (const ui::InputDevice& touchpad :
       ui::DeviceDataManager::GetInstance()->GetTouchpadDevices()) {
    if (touchpad.type == ui::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

bool InternalInputDevicesEventBlocker::HasInternalKeyboard() {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type == ui::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

void InternalInputDevicesEventBlocker::UpdateInternalTouchpad(
    bool should_be_blocked) {
  if (should_be_blocked == is_touchpad_blocked_)
    return;

  if (HasInternalTouchpad()) {
    Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
        !should_be_blocked, TouchDeviceEnabledSource::GLOBAL);
    is_touchpad_blocked_ = should_be_blocked;
    VLOG(1) << "Internal touchpad is blocked: " << is_touchpad_blocked_;
  }
}

void InternalInputDevicesEventBlocker::UpdateInternalKeyboard(
    bool should_be_blocked) {
  if (should_be_blocked == is_keyboard_blocked_)
    return;

  if (!HasInternalKeyboard())
    return;

  // Block or unblock the internal keyboard.
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  std::vector<ui::DomCode> allowed_keys;
  if (should_be_blocked) {
    // Only allow the acccessible keys present on the side of some devices to
    // continue working if the internal keyboard events should be blocked.
    allowed_keys.push_back(ui::DomCode::VOLUME_DOWN);
    allowed_keys.push_back(ui::DomCode::VOLUME_UP);
    allowed_keys.push_back(ui::DomCode::POWER);
  }
  input_controller->SetInternalKeyboardFilter(should_be_blocked, allowed_keys);
  is_keyboard_blocked_ = should_be_blocked;
  VLOG(1) << "Internal keyboard is blocked: " << is_keyboard_blocked_;
}

}  // namespace ash
