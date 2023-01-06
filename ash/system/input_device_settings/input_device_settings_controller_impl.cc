// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <iterator>
#include <memory>
#include <vector>

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_pref_manager.h"
#include "ash/system/input_device_settings/input_device_pref_manager_impl.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/identity.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {
using DeviceId = InputDeviceSettingsController::DeviceId;

DeviceId ExtractDeviceIdFromInputDevice(const ui::InputDevice& device) {
  return device.id;
}

template <typename T>
DeviceId ExtractDeviceIdFromDeviceMapPair(
    const std::pair<DeviceId, T>& id_t_pair) {
  return id_t_pair.first;
}

template <class DeviceMojomPtr>
void GetAddedAndRemovedDevices(
    std::vector<ui::InputDevice> devices,
    const base::flat_map<DeviceId, DeviceMojomPtr>& connected_devices,
    std::vector<ui::InputDevice>* devices_to_add,
    std::vector<DeviceId>* devices_to_remove) {
  // Sort input device list by id as |base::ranges::set_difference| requires
  // input lists are sorted.
  base::ranges::sort(devices, base::ranges::less(),
                     ExtractDeviceIdFromInputDevice);

  // Generate a vector with only the device ids froom the connected_devices map.
  std::vector<DeviceId> connected_devices_ids;
  connected_devices_ids.reserve(connected_devices.size());
  base::ranges::transform(connected_devices,
                          std::back_inserter(connected_devices_ids),
                          ExtractDeviceIdFromDeviceMapPair<DeviceMojomPtr>);

  // Generate a list of devices in |devices| but missing from
  // |connected_devices|.
  base::ranges::set_difference(
      devices, connected_devices_ids, std::back_inserter(*devices_to_add),
      /*Comp=*/base::ranges::less(), /*Proj1=*/ExtractDeviceIdFromInputDevice);

  // Generate a list of ids for devices which are in |connected_devices| but
  // missing from |devices|.
  base::ranges::set_difference(
      connected_devices_ids, devices, std::back_inserter(*devices_to_remove),
      /*Comp=*/base::ranges::less(), /*Proj1=*/base::identity(),
      /*Proj2=*/ExtractDeviceIdFromInputDevice);
}

mojom::KeyboardPtr BuildMojomKeyboard(const ui::InputDevice& keyboard) {
  // TODO(dpad): Fully initialize the mojom::Keyboard object.
  mojom::KeyboardPtr mojom_keyboard = mojom::Keyboard::New();
  mojom_keyboard->id = keyboard.id;
  mojom_keyboard->name = keyboard.name;
  return mojom_keyboard;
}
}  // namespace

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl()
    : pref_manager_(std::make_unique<InputDevicePrefManagerImpl>()) {
  Init();
}

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl(
    std::unique_ptr<InputDevicePrefManager> pref_manager)
    : pref_manager_(std::move(pref_manager)) {
  Init();
}

void InputDeviceSettingsControllerImpl::Init() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

std::vector<mojom::KeyboardPtr>
InputDeviceSettingsControllerImpl::GetConnectedKeyboards() {
  std::vector<mojom::KeyboardPtr> keyboard_vector;
  keyboard_vector.reserve(keyboards_.size());

  for (const auto& [_, keyboard] : keyboards_) {
    keyboard_vector.push_back(keyboard->Clone());
  }

  return keyboard_vector;
}

// TODO(dpad): Implement updating of keyboard settings.
void InputDeviceSettingsControllerImpl::SetKeyboardSettings(
    DeviceId id,
    const mojom::KeyboardSettings& settings) {
  NOTIMPLEMENTED();
}

void InputDeviceSettingsControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceSettingsControllerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// TODO(dpad): Implement pulling of device lists.
void InputDeviceSettingsControllerImpl::RefreshDeviceLists() {
  RefreshKeyboardList();
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardConnected(DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardConnected(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardDisconnected(
    DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardDisconnected(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::RefreshKeyboardList() {
  std::vector<ui::InputDevice> keyboards_to_add;
  std::vector<DeviceId> keyboard_ids_to_remove;

  GetAddedAndRemovedDevices(
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices(), keyboards_,
      &keyboards_to_add, &keyboard_ids_to_remove);

  for (const auto& keyboard : keyboards_to_add) {
    // Get initial settings from the pref manager and generate our local storage
    // of the device.
    mojom::KeyboardPtr mojom_keyboard = BuildMojomKeyboard(keyboard);
    pref_manager_->InitializeKeyboardSettings(mojom_keyboard.get());
    keyboards_.insert_or_assign(keyboard.id, std::move(mojom_keyboard));
    DispatchKeyboardConnected(keyboard.id);
  }

  for (const auto id : keyboard_ids_to_remove) {
    DispatchKeyboardDisconnected(id);
    keyboards_.erase(id);
  }
}

void InputDeviceSettingsControllerImpl::OnInputDeviceConfigurationChanged(
    uint8_t input_device_type) {
  RefreshDeviceLists();
}

void InputDeviceSettingsControllerImpl::OnDeviceListsComplete() {
  RefreshDeviceLists();
}

}  // namespace ash
