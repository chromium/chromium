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
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {
mojom::KeyboardPtr BuildMojomKeyboard(const ui::InputDevice& keyboard) {
  // TODO(dpad): Fully initialize the mojom::Keyboard object.
  mojom::KeyboardPtr mojom_keyboard = mojom::Keyboard::New();
  mojom_keyboard->id = keyboard.id;
  mojom_keyboard->name = keyboard.name;
  mojom_keyboard->device_key =
      InputDevicePrefManagerImpl::BuildDeviceKey(keyboard);
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
  keyboard_notifier_ =
      std::make_unique<InputDeviceNotifier<mojom::KeyboardPtr>>(
          &keyboards_,
          base::BindRepeating(
              &InputDeviceSettingsControllerImpl::OnKeyboardListUpdated,
              base::Unretained(this)));
}

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  // Manually reset `keyboard_notifier_` to avoid any race conditions between
  // `keyboard_notifier_` and `keyboards_`.
  keyboard_notifier_.reset();
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

void InputDeviceSettingsControllerImpl::OnKeyboardListUpdated(
    std::vector<ui::InputDevice> keyboards_to_add,
    std::vector<DeviceId> keyboard_ids_to_remove) {
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

}  // namespace ash
