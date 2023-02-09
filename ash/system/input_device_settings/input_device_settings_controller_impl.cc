// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <iterator>
#include <memory>
#include <vector>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {
mojom::KeyboardPtr BuildMojomKeyboard(const ui::InputDevice& keyboard) {
  // TODO(dpad): Fully initialize the mojom::Keyboard object.
  mojom::KeyboardPtr mojom_keyboard = mojom::Keyboard::New();
  mojom_keyboard->id = keyboard.id;
  mojom_keyboard->name = keyboard.name;
  mojom_keyboard->device_key = BuildDeviceKey(keyboard);
  return mojom_keyboard;
}

mojom::MousePtr BuildMojomMouse(const ui::InputDevice& mouse) {
  // TODO(dpad): Fully initialize the objects.
  mojom::MousePtr mojom_mouse = mojom::Mouse::New();
  mojom_mouse->id = mouse.id;
  mojom_mouse->name = mouse.name;
  mojom_mouse->device_key = BuildDeviceKey(mouse);
  return mojom_mouse;
}

mojom::TouchpadPtr BuildMojomTouchpad(const ui::InputDevice& touchpad) {
  // TODO(dpad): Fully initialize the objects.
  mojom::TouchpadPtr mojom_touchpad = mojom::Touchpad::New();
  mojom_touchpad->id = touchpad.id;
  mojom_touchpad->name = touchpad.name;
  mojom_touchpad->device_key = BuildDeviceKey(touchpad);
  return mojom_touchpad;
}

mojom::PointingStickPtr BuildMojomPointingStick(
    const ui::InputDevice& touchpad) {
  // TODO(dpad): Fully initialize the objects.
  mojom::PointingStickPtr mojom_pointing_stick = mojom::PointingStick::New();
  mojom_pointing_stick->id = touchpad.id;
  mojom_pointing_stick->name = touchpad.name;
  mojom_pointing_stick->device_key = BuildDeviceKey(touchpad);
  return mojom_pointing_stick;
}
}  // namespace

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl()
    : keyboard_pref_handler_(std::make_unique<KeyboardPrefHandlerImpl>()) {
  Init();
}

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl(
    std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler)
    : keyboard_pref_handler_(std::move(keyboard_pref_handler)) {
  Init();
}

void InputDeviceSettingsControllerImpl::Init() {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->AddObserver(this);
  }
  keyboard_notifier_ =
      std::make_unique<InputDeviceNotifier<mojom::KeyboardPtr>>(
          &keyboards_,
          base::BindRepeating(
              &InputDeviceSettingsControllerImpl::OnKeyboardListUpdated,
              base::Unretained(this)));
  mouse_notifier_ = std::make_unique<InputDeviceNotifier<mojom::MousePtr>>(
      &mice_, base::BindRepeating(
                  &InputDeviceSettingsControllerImpl::OnMouseListUpdated,
                  base::Unretained(this)));
  touchpad_notifier_ =
      std::make_unique<InputDeviceNotifier<mojom::TouchpadPtr>>(
          &touchpads_,
          base::BindRepeating(
              &InputDeviceSettingsControllerImpl::OnTouchpadListUpdated,
              base::Unretained(this)));
  pointing_stick_notifier_ =
      std::make_unique<InputDeviceNotifier<mojom::PointingStickPtr>>(
          &pointing_sticks_,
          base::BindRepeating(
              &InputDeviceSettingsControllerImpl::OnPointingStickListUpdated,
              base::Unretained(this)));
}

InputDeviceSettingsControllerImpl::~InputDeviceSettingsControllerImpl() {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->RemoveObserver(this);
  }
}

void InputDeviceSettingsControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterDictionaryPref(prefs::kKeyboardDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(prefs::kMouseDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(
      prefs::kPointingStickDeviceSettingsDictPref);
  pref_registry->RegisterDictionaryPref(prefs::kTouchpadDeviceSettingsDictPref);
}

void InputDeviceSettingsControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_pref_service_ = pref_service;
  // TODO(michaelcheco): Initialize settings and notify observers.
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

std::vector<mojom::TouchpadPtr>
InputDeviceSettingsControllerImpl::GetConnectedTouchpads() {
  std::vector<mojom::TouchpadPtr> mouse_vector;
  mouse_vector.reserve(touchpads_.size());

  for (const auto& [_, touchpad] : touchpads_) {
    mouse_vector.push_back(touchpad->Clone());
  }

  return mouse_vector;
}

std::vector<mojom::MousePtr>
InputDeviceSettingsControllerImpl::GetConnectedMice() {
  std::vector<mojom::MousePtr> mouse_vector;
  mouse_vector.reserve(mice_.size());

  for (const auto& [_, mouse] : mice_) {
    mouse_vector.push_back(mouse->Clone());
  }

  return mouse_vector;
}

std::vector<mojom::PointingStickPtr>
InputDeviceSettingsControllerImpl::GetConnectedPointingSticks() {
  std::vector<mojom::PointingStickPtr> pointing_stick_vector;
  pointing_stick_vector.reserve(pointing_sticks_.size());

  for (const auto& [_, pointing_stick] : pointing_sticks_) {
    pointing_stick_vector.push_back(pointing_stick->Clone());
  }

  return pointing_stick_vector;
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

void InputDeviceSettingsControllerImpl::DispatchTouchpadConnected(DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadConnected(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadDisconnected(
    DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadDisconnected(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseConnected(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseConnected(mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseDisconnected(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseDisconnected(mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchPointingStickConnected(
    DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  const auto& pointing_stick = *pointing_sticks_.at(id);
  for (auto& observer : observers_) {
    observer.OnPointingStickConnected(pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::DispatchPointingStickDisconnected(
    DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  const auto& pointing_stick = *pointing_sticks_.at(id);
  for (auto& observer : observers_) {
    observer.OnPointingStickDisconnected(pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::OnKeyboardListUpdated(
    std::vector<ui::InputDevice> keyboards_to_add,
    std::vector<DeviceId> keyboard_ids_to_remove) {
  for (const auto& keyboard : keyboards_to_add) {
    // Get initial settings from the pref manager and generate our local storage
    // of the device.
    auto mojom_keyboard = BuildMojomKeyboard(keyboard);
    if (active_pref_service_) {
      keyboard_pref_handler_->InitializeKeyboardSettings(active_pref_service_,
                                                         mojom_keyboard.get());
    }
    keyboards_.insert_or_assign(keyboard.id, std::move(mojom_keyboard));
    DispatchKeyboardConnected(keyboard.id);
  }

  for (const auto id : keyboard_ids_to_remove) {
    DispatchKeyboardDisconnected(id);
    keyboards_.erase(id);
  }
}

void InputDeviceSettingsControllerImpl::OnTouchpadListUpdated(
    std::vector<ui::InputDevice> touchpads_to_add,
    std::vector<DeviceId> touchpad_ids_to_remove) {
  for (const auto& touchpad : touchpads_to_add) {
    auto mojom_touchpad = BuildMojomTouchpad(touchpad);
    touchpads_.insert_or_assign(touchpad.id, std::move(mojom_touchpad));
    DispatchTouchpadConnected(touchpad.id);
  }

  for (const auto id : touchpad_ids_to_remove) {
    DispatchTouchpadDisconnected(id);
    touchpads_.erase(id);
  }
}

void InputDeviceSettingsControllerImpl::OnMouseListUpdated(
    std::vector<ui::InputDevice> mice_to_add,
    std::vector<DeviceId> mouse_ids_to_remove) {
  for (const auto& mouse : mice_to_add) {
    auto mojom_mouse = BuildMojomMouse(mouse);
    mice_.insert_or_assign(mouse.id, std::move(mojom_mouse));
    DispatchMouseConnected(mouse.id);
  }

  for (const auto id : mouse_ids_to_remove) {
    DispatchMouseDisconnected(id);
    mice_.erase(id);
  }
}

void InputDeviceSettingsControllerImpl::OnPointingStickListUpdated(
    std::vector<ui::InputDevice> pointing_sticks_to_add,
    std::vector<DeviceId> pointing_stick_ids_to_remove) {
  for (const auto& pointing_stick : pointing_sticks_to_add) {
    auto mojom_pointing_stick = BuildMojomPointingStick(pointing_stick);
    pointing_sticks_.insert_or_assign(pointing_stick.id,
                                      std::move(mojom_pointing_stick));
    DispatchPointingStickConnected(pointing_stick.id);
  }

  for (const auto id : pointing_stick_ids_to_remove) {
    DispatchPointingStickDisconnected(id);
    pointing_sticks_.erase(id);
  }
}

void InputDeviceSettingsControllerImpl::SetPrefHandlersForTesting(
    std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler) {
  keyboard_pref_handler_ = std::move(keyboard_pref_handler);
}

}  // namespace ash
