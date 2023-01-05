// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <vector>

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/notreached.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

// TODO(dpad): Implement retrieval of connected keyboards.
std::vector<mojom::KeyboardPtr>
InputDeviceSettingsControllerImpl::GetConnectedKeyboards() {
  NOTIMPLEMENTED();
  return {};
}

// TODO(dpad): Implement updating of keyboard settings.
void InputDeviceSettingsControllerImpl::SetKeyboardSettings(
    DeviceId id,
    const mojom::KeyboardSettings& settings) {
  NOTIMPLEMENTED();
}

// TODO(dpad): Implement adding/removing of observers.
void InputDeviceSettingsControllerImpl::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

// TODO(dpad): Implement adding/removing of observers.
void InputDeviceSettingsControllerImpl::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

// TODO(dpad@): Implement pulling of device lists.
void InputDeviceSettingsControllerImpl::RefreshDeviceLists() {
  NOTIMPLEMENTED();
}

void InputDeviceSettingsControllerImpl::OnInputDeviceConfigurationChanged(
    uint8_t input_device_type) {
  RefreshDeviceLists();
}

void InputDeviceSettingsControllerImpl::OnDeviceListsComplete() {
  RefreshDeviceLists();
}

}  // namespace ash
