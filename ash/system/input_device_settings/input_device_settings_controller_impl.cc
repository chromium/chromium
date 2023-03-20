// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <iterator>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

mojom::MetaKey GetMetaKeyForKeyboard(const ui::InputDevice& keyboard) {
  const auto device_type =
      Shell::Get()->keyboard_capability()->GetDeviceType(keyboard);
  switch (device_type) {
    case ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case ui::KeyboardCapability::DeviceType::kDeviceUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
      return Shell::Get()->keyboard_capability()->HasLauncherButton(keyboard)
                 ? mojom::MetaKey::kLauncher
                 : mojom::MetaKey::kSearch;
    case ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      return mojom::MetaKey::kCommand;
    case ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown:
      return mojom::MetaKey::kExternalMeta;
  };
}

mojom::KeyboardPtr BuildMojomKeyboard(const ui::InputDevice& keyboard) {
  mojom::KeyboardPtr mojom_keyboard = mojom::Keyboard::New();
  mojom_keyboard->id = keyboard.id;
  mojom_keyboard->name = keyboard.name;
  mojom_keyboard->device_key = BuildDeviceKey(keyboard);
  mojom_keyboard->is_external =
      keyboard.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  // Enable only when flag is enabled to avoid crashing while problem is
  // addressed. See b/272960076
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    mojom_keyboard->modifier_keys =
        Shell::Get()->keyboard_capability()->GetModifierKeys(keyboard);
    mojom_keyboard->meta_key = GetMetaKeyForKeyboard(keyboard);
  }
  return mojom_keyboard;
}

mojom::MousePtr BuildMojomMouse(const ui::InputDevice& mouse) {
  mojom::MousePtr mojom_mouse = mojom::Mouse::New();
  mojom_mouse->id = mouse.id;
  mojom_mouse->name = mouse.name;
  mojom_mouse->device_key = BuildDeviceKey(mouse);
  mojom_mouse->is_external =
      mouse.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  return mojom_mouse;
}

mojom::TouchpadPtr BuildMojomTouchpad(const ui::InputDevice& touchpad) {
  mojom::TouchpadPtr mojom_touchpad = mojom::Touchpad::New();
  mojom_touchpad->id = touchpad.id;
  mojom_touchpad->name = touchpad.name;
  mojom_touchpad->device_key = BuildDeviceKey(touchpad);
  mojom_touchpad->is_external =
      touchpad.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  return mojom_touchpad;
}

mojom::PointingStickPtr BuildMojomPointingStick(
    const ui::InputDevice& touchpad) {
  mojom::PointingStickPtr mojom_pointing_stick = mojom::PointingStick::New();
  mojom_pointing_stick->id = touchpad.id;
  mojom_pointing_stick->name = touchpad.name;
  mojom_pointing_stick->device_key = BuildDeviceKey(touchpad);
  mojom_pointing_stick->is_external =
      touchpad.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  return mojom_pointing_stick;
}
}  // namespace

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl()
    : keyboard_pref_handler_(std::make_unique<KeyboardPrefHandlerImpl>()),
      touchpad_pref_handler_(std::make_unique<TouchpadPrefHandlerImpl>()),
      mouse_pref_handler_(std::make_unique<MousePrefHandlerImpl>()),
      pointing_stick_pref_handler_(
          std::make_unique<PointingStickPrefHandlerImpl>()),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  Init();
}

InputDeviceSettingsControllerImpl::InputDeviceSettingsControllerImpl(
    std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler,
    std::unique_ptr<TouchpadPrefHandler> touchpad_pref_handler,
    std::unique_ptr<MousePrefHandler> mouse_pref_handler,
    std::unique_ptr<PointingStickPrefHandler> pointing_stick_pref_handler,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : keyboard_pref_handler_(std::move(keyboard_pref_handler)),
      touchpad_pref_handler_(std::move(touchpad_pref_handler)),
      mouse_pref_handler_(std::move(mouse_pref_handler)),
      pointing_stick_pref_handler_(std::move(pointing_stick_pref_handler)),
      sequenced_task_runner_(std::move(task_runner)) {
  Init();
}

void InputDeviceSettingsControllerImpl::Init() {
  Shell::Get()->session_controller()->AddObserver(this);
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
  Shell::Get()->session_controller()->RemoveObserver(this);
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
  // If the flag is disabled, clear all the settings dictionaries.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    active_pref_service_ = nullptr;
    pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kMouseDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kPointingStickDeviceSettingsDictPref, {});
    pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref, {});
    return;
  }
  active_pref_service_ = pref_service;

  // Device settings must be refreshed when the user pref service is updated,
  // but all dependencies of `InputDeviceSettingsControllerImpl` must be updated
  // due to the active pref service change first. Therefore, schedule a task so
  // other dependencies are updated first.
  if (!settings_refresh_pending_) {
    settings_refresh_pending_ = true;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InputDeviceSettingsControllerImpl::RefreshAllDeviceSettings,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void InputDeviceSettingsControllerImpl::RefreshAllDeviceSettings() {
  settings_refresh_pending_ = false;
  for (const auto& [id, keyboard] : keyboards_) {
    keyboard_pref_handler_->InitializeKeyboardSettings(active_pref_service_,
                                                       keyboard.get());
    DispatchKeyboardSettingsChanged(id);
  }
  for (const auto& [id, touchpad] : touchpads_) {
    touchpad_pref_handler_->InitializeTouchpadSettings(active_pref_service_,
                                                       touchpad.get());
    DispatchTouchpadSettingsChanged(id);
  }
  for (const auto& [id, mouse] : mice_) {
    mouse_pref_handler_->InitializeMouseSettings(active_pref_service_,
                                                 mouse.get());
    DispatchMouseSettingsChanged(id);
  }
  for (const auto& [id, pointing_stick] : pointing_sticks_) {
    pointing_stick_pref_handler_->InitializePointingStickSettings(
        active_pref_service_, pointing_stick.get());
    DispatchPointingStickSettingsChanged(id);
  }
}

const mojom::KeyboardSettings*
InputDeviceSettingsControllerImpl::GetKeyboardSettings(DeviceId id) {
  auto iter = keyboards_.find(id);
  if (iter == keyboards_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::MouseSettings* InputDeviceSettingsControllerImpl::GetMouseSettings(
    DeviceId id) {
  auto iter = mice_.find(id);
  if (iter == mice_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::TouchpadSettings*
InputDeviceSettingsControllerImpl::GetTouchpadSettings(DeviceId id) {
  auto iter = touchpads_.find(id);
  if (iter == touchpads_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
}

const mojom::PointingStickSettings*
InputDeviceSettingsControllerImpl::GetPointingStickSettings(DeviceId id) {
  auto iter = pointing_sticks_.find(id);
  if (iter == pointing_sticks_.end()) {
    return nullptr;
  }
  return iter->second->settings.get();
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

void InputDeviceSettingsControllerImpl::SetKeyboardSettings(
    DeviceId id,
    mojom::KeyboardSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_keyboard_iter = keyboards_.find(id);
  if (found_keyboard_iter == keyboards_.end()) {
    return;
  }

  // TODO(dpad): Validate incoming settings to make sure the settings can apply
  // to the given device.
  auto& found_keyboard = *found_keyboard_iter->second;
  found_keyboard.settings = settings.Clone();
  keyboard_pref_handler_->UpdateKeyboardSettings(active_pref_service_,
                                                 found_keyboard);
  DispatchKeyboardSettingsChanged(id);
  // Check the list of keyboards to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, keyboard] : keyboards_) {
    if (device_id != found_keyboard.id &&
        keyboard->device_key == found_keyboard.device_key) {
      keyboard->settings = settings->Clone();
      DispatchKeyboardSettingsChanged(device_id);
    }
  }
}

void InputDeviceSettingsControllerImpl::SetTouchpadSettings(
    DeviceId id,
    mojom::TouchpadSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_touchpad_iter = touchpads_.find(id);
  if (found_touchpad_iter == touchpads_.end()) {
    return;
  }

  // TODO(dpad): Validate incoming settings to make sure the settings can apply
  // to the given device.
  auto& found_touchpad = *found_touchpad_iter->second;
  found_touchpad.settings = settings.Clone();
  touchpad_pref_handler_->UpdateTouchpadSettings(active_pref_service_,
                                                 found_touchpad);
  DispatchTouchpadSettingsChanged(id);
  // Check the list of touchpads to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, touchpad] : touchpads_) {
    if (device_id != found_touchpad.id &&
        touchpad->device_key == found_touchpad.device_key) {
      touchpad->settings = settings->Clone();
      DispatchTouchpadSettingsChanged(device_id);
    }
  }
}

void InputDeviceSettingsControllerImpl::SetMouseSettings(
    DeviceId id,
    mojom::MouseSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_mouse_iter = mice_.find(id);
  if (found_mouse_iter == mice_.end()) {
    return;
  }

  // TODO(dpad): Validate incoming settings to make sure the settings can apply
  // to the given device.
  auto& found_mouse = *found_mouse_iter->second;
  found_mouse.settings = settings.Clone();
  mouse_pref_handler_->UpdateMouseSettings(active_pref_service_, found_mouse);
  DispatchMouseSettingsChanged(id);
  // Check the list of mice to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, mouse] : mice_) {
    if (device_id != found_mouse.id &&
        mouse->device_key == found_mouse.device_key) {
      mouse->settings = settings->Clone();
      DispatchMouseSettingsChanged(device_id);
    }
  }
}

void InputDeviceSettingsControllerImpl::SetPointingStickSettings(
    DeviceId id,
    mojom::PointingStickSettingsPtr settings) {
  DCHECK(active_pref_service_);

  // If a device with the given id does not exist, do nothing.
  auto found_pointing_stick_iter = pointing_sticks_.find(id);
  if (found_pointing_stick_iter == pointing_sticks_.end()) {
    return;
  }

  // TODO(dpad): Validate incoming settings to make sure the settings can apply
  // to the given device.
  auto& found_pointing_stick = *found_pointing_stick_iter->second;
  found_pointing_stick.settings = settings.Clone();
  pointing_stick_pref_handler_->UpdatePointingStickSettings(
      active_pref_service_, found_pointing_stick);
  DispatchPointingStickSettingsChanged(id);
  // Check the list of pointing sticks to see if any have the same |device_key|.
  // If so, their settings need to also be updated.
  for (const auto& [device_id, pointing_stick] : pointing_sticks_) {
    if (device_id != found_pointing_stick.id &&
        pointing_stick->device_key == found_pointing_stick.device_key) {
      pointing_stick->settings = settings->Clone();
      DispatchPointingStickSettingsChanged(device_id);
    }
  }
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

void InputDeviceSettingsControllerImpl::
    DispatchKeyboardDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  auto keyboard_iter = keyboards_.find(id);
  auto keyboard = std::move(keyboard_iter->second);
  keyboards_.erase(keyboard_iter);
  for (auto& observer : observers_) {
    observer.OnKeyboardDisconnected(*keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchKeyboardSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(keyboards_, id));
  const auto& keyboard = *keyboards_.at(id);
  for (auto& observer : observers_) {
    observer.OnKeyboardSettingsUpdated(keyboard);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadConnected(DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadConnected(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchTouchpadDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  auto touchpad_iter = touchpads_.find(id);
  auto touchpad = std::move(touchpad_iter->second);
  touchpads_.erase(touchpad_iter);
  for (auto& observer : observers_) {
    observer.OnTouchpadDisconnected(*touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchTouchpadSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(touchpads_, id));
  const auto& touchpad = *touchpads_.at(id);
  for (auto& observer : observers_) {
    observer.OnTouchpadSettingsUpdated(touchpad);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseConnected(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseConnected(mouse);
  }
}

void InputDeviceSettingsControllerImpl::
    DispatchMouseDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  auto mouse_iter = mice_.find(id);
  auto mouse = std::move(mouse_iter->second);
  mice_.erase(mouse_iter);
  for (auto& observer : observers_) {
    observer.OnMouseDisconnected(*mouse);
  }
}

void InputDeviceSettingsControllerImpl::DispatchMouseSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(mice_, id));
  const auto& mouse = *mice_.at(id);
  for (auto& observer : observers_) {
    observer.OnMouseSettingsUpdated(mouse);
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

void InputDeviceSettingsControllerImpl::
    DispatchPointingStickDisconnectedAndEraseFromList(DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  auto pointing_stick_iter = pointing_sticks_.find(id);
  auto pointing_stick = std::move(pointing_stick_iter->second);
  pointing_sticks_.erase(pointing_stick_iter);
  for (auto& observer : observers_) {
    observer.OnPointingStickDisconnected(*pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::DispatchPointingStickSettingsChanged(
    DeviceId id) {
  DCHECK(base::Contains(pointing_sticks_, id));
  const auto& pointing_stick = *pointing_sticks_.at(id);
  for (auto& observer : observers_) {
    observer.OnPointingStickSettingsUpdated(pointing_stick);
  }
}

void InputDeviceSettingsControllerImpl::OnKeyboardListUpdated(
    std::vector<ui::InputDevice> keyboards_to_add,
    std::vector<DeviceId> keyboard_ids_to_remove) {
  for (const auto& keyboard : keyboards_to_add) {
    // Get initial settings from the pref manager and generate our local storage
    // of the device.
    auto mojom_keyboard = BuildMojomKeyboard(keyboard);
    keyboard_pref_handler_->InitializeKeyboardSettings(active_pref_service_,
                                                       mojom_keyboard.get());
    keyboards_.insert_or_assign(keyboard.id, std::move(mojom_keyboard));
    DispatchKeyboardConnected(keyboard.id);
  }

  for (const auto id : keyboard_ids_to_remove) {
    DispatchKeyboardDisconnectedAndEraseFromList(id);
  }
}

void InputDeviceSettingsControllerImpl::OnTouchpadListUpdated(
    std::vector<ui::InputDevice> touchpads_to_add,
    std::vector<DeviceId> touchpad_ids_to_remove) {
  for (const auto& touchpad : touchpads_to_add) {
    auto mojom_touchpad = BuildMojomTouchpad(touchpad);
    touchpad_pref_handler_->InitializeTouchpadSettings(active_pref_service_,
                                                       mojom_touchpad.get());
    touchpads_.insert_or_assign(touchpad.id, std::move(mojom_touchpad));
    DispatchTouchpadConnected(touchpad.id);
  }

  for (const auto id : touchpad_ids_to_remove) {
    DispatchTouchpadDisconnectedAndEraseFromList(id);
  }
}

void InputDeviceSettingsControllerImpl::OnMouseListUpdated(
    std::vector<ui::InputDevice> mice_to_add,
    std::vector<DeviceId> mouse_ids_to_remove) {
  for (const auto& mouse : mice_to_add) {
    auto mojom_mouse = BuildMojomMouse(mouse);
    mouse_pref_handler_->InitializeMouseSettings(active_pref_service_,
                                                 mojom_mouse.get());
    mice_.insert_or_assign(mouse.id, std::move(mojom_mouse));
    DispatchMouseConnected(mouse.id);
  }

  for (const auto id : mouse_ids_to_remove) {
    DispatchMouseDisconnectedAndEraseFromList(id);
  }
}

void InputDeviceSettingsControllerImpl::OnPointingStickListUpdated(
    std::vector<ui::InputDevice> pointing_sticks_to_add,
    std::vector<DeviceId> pointing_stick_ids_to_remove) {
  for (const auto& pointing_stick : pointing_sticks_to_add) {
    auto mojom_pointing_stick = BuildMojomPointingStick(pointing_stick);
    pointing_stick_pref_handler_->InitializePointingStickSettings(
        active_pref_service_, mojom_pointing_stick.get());
    pointing_sticks_.insert_or_assign(pointing_stick.id,
                                      std::move(mojom_pointing_stick));
    DispatchPointingStickConnected(pointing_stick.id);
  }

  for (const auto id : pointing_stick_ids_to_remove) {
    DispatchPointingStickDisconnectedAndEraseFromList(id);
  }
}

}  // namespace ash
