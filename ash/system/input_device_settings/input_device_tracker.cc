// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_key_alias_manager.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/contains.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

InputDeviceTracker::InputDeviceTracker() {
  Shell::Get()->session_controller()->AddObserver(this);
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->input_device_settings_controller()->AddObserver(this);
  }
}

InputDeviceTracker::~InputDeviceTracker() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->input_device_settings_controller()->RemoveObserver(this);
  }
}

void InputDeviceTracker::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterListPref(prefs::kMouseObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kKeyboardObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kTouchpadObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kPointingStickObservedDevicesPref);
}

void InputDeviceTracker::ResetPrefMembers() {
  mouse_observed_devices_ = std::make_unique<StringListPrefMember>();
  touchpad_observed_devices_ = std::make_unique<StringListPrefMember>();
  keyboard_observed_devices_ = std::make_unique<StringListPrefMember>();
  pointing_stick_observed_devices_ = std::make_unique<StringListPrefMember>();
}

void InputDeviceTracker::OnKeyboardConnected(const mojom::Keyboard& keyboard) {
  RecordDeviceConnected(InputDeviceCategory::kKeyboard, keyboard.device_key);
}

void InputDeviceTracker::OnTouchpadConnected(const mojom::Touchpad& touchpad) {
  RecordDeviceConnected(InputDeviceCategory::kTouchpad, touchpad.device_key);
}

void InputDeviceTracker::OnMouseConnected(const mojom::Mouse& mouse) {
  RecordDeviceConnected(InputDeviceCategory::kMouse, mouse.device_key);
}

void InputDeviceTracker::OnPointingStickConnected(
    const mojom::PointingStick& pointing_stick) {
  RecordDeviceConnected(InputDeviceCategory::kPointingStick,
                        pointing_stick.device_key);
}

void InputDeviceTracker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // When the user's `pref_service` changes, we need to re-initialize our
  // `StringListPrefMember`s and record that we have seen all currently
  // connected devices.
  Init(pref_service);
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    RecordConnectedDevices();
  }
}

bool InputDeviceTracker::WasDevicePreviouslyConnected(
    InputDeviceCategory category,
    std::string_view device_key) const {
  const auto* observed_devices = GetObservedDevicesForCategory(category);
  return observed_devices
             ? base::Contains(observed_devices->GetValue(), device_key)
             : false;
}

void InputDeviceTracker::RecordConnectedDevices() {
  const auto keyboards =
      Shell::Get()->input_device_settings_controller()->GetConnectedKeyboards();
  for (const auto& keyboard : keyboards) {
    OnKeyboardConnected(*keyboard);
  }

  const auto touchpads =
      Shell::Get()->input_device_settings_controller()->GetConnectedTouchpads();
  for (const auto& touchpad : touchpads) {
    OnTouchpadConnected(*touchpad);
  }

  const auto mice =
      Shell::Get()->input_device_settings_controller()->GetConnectedMice();
  for (const auto& mouse : mice) {
    OnMouseConnected(*mouse);
  }

  const auto pointing_sticks = Shell::Get()
                                   ->input_device_settings_controller()
                                   ->GetConnectedPointingSticks();
  for (const auto& pointing_stick : pointing_sticks) {
    OnPointingStickConnected(*pointing_stick);
  }
}

void InputDeviceTracker::Init(PrefService* pref_service) {
  ResetPrefMembers();

  mouse_observed_devices_->Init(prefs::kMouseObservedDevicesPref, pref_service);
  touchpad_observed_devices_->Init(prefs::kTouchpadObservedDevicesPref,
                                   pref_service);
  keyboard_observed_devices_->Init(prefs::kKeyboardObservedDevicesPref,
                                   pref_service);
  pointing_stick_observed_devices_->Init(
      prefs::kPointingStickObservedDevicesPref, pref_service);
}

void InputDeviceTracker::RecordDeviceConnected(InputDeviceCategory category,
                                               std::string_view device_key) {
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }

  auto* const observed_devices = GetObservedDevicesForCategory(category);
  // If `observed_devices` is null, that means we are not yet in a valid chrome
  // session.
  if (!observed_devices) {
    return;
  }

  std::vector<std::string> previously_observed_devices =
      observed_devices->GetValue();

  if (!base::Contains(previously_observed_devices, device_key) &&
      !HasSeenPrimaryDeviceKeyAlias(previously_observed_devices, device_key)) {
    previously_observed_devices.emplace_back(device_key);
    observed_devices->SetValue(previously_observed_devices);
  }
}

bool InputDeviceTracker::HasSeenPrimaryDeviceKeyAlias(
    const std::vector<std::string>& previously_observed_devices,
    std::string_view device_key) {
  const auto* aliases = Shell::Get()
                            ->input_device_key_alias_manager()
                            ->GetAliasesForPrimaryDeviceKey(device_key);
  if (!aliases) {
    return false;
  }

  for (const auto& alias : *aliases) {
    if (base::Contains(previously_observed_devices, alias)) {
      return true;
    }
  }
  return false;
}

StringListPrefMember* InputDeviceTracker::GetObservedDevicesForCategory(
    InputDeviceCategory category) const {
  switch (category) {
    case InputDeviceCategory::kMouse:
      return mouse_observed_devices_.get();
    case InputDeviceCategory::kKeyboard:
      return keyboard_observed_devices_.get();
    case InputDeviceCategory::kPointingStick:
      return pointing_stick_observed_devices_.get();
    case InputDeviceCategory::kTouchpad:
      return touchpad_observed_devices_.get();
  }
}

}  // namespace ash
