// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

InputDeviceTracker::InputDeviceTracker() {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->AddObserver(this);
    Shell::Get()->input_device_settings_controller()->AddObserver(this);
  }
}

InputDeviceTracker::~InputDeviceTracker() {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->RemoveObserver(this);
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

void InputDeviceTracker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // When the user's `pref_service` changes, we need to re-initialize our
  // `StringListPrefMember`s and record that we have seen all currently
  // connected devices.
  Init(pref_service);
  RecordConnectedDevices();
}

void InputDeviceTracker::RecordConnectedDevices() {
  const auto keyboards =
      Shell::Get()->input_device_settings_controller()->GetConnectedKeyboards();
  for (const auto& keyboard : keyboards) {
    OnKeyboardConnected(*keyboard);
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

void InputDeviceTracker::RecordDeviceConnected(
    InputDeviceCategory category,
    const base::StringPiece& device_key) {
  StringListPrefMember* observed_devices = nullptr;
  switch (category) {
    case InputDeviceCategory::kMouse:
      observed_devices = mouse_observed_devices_.get();
      break;
    case InputDeviceCategory::kKeyboard:
      observed_devices = keyboard_observed_devices_.get();
      break;
    case InputDeviceCategory::kPointingStick:
      observed_devices = pointing_stick_observed_devices_.get();
      break;
    case InputDeviceCategory::kTouchpad:
      observed_devices = touchpad_observed_devices_.get();
      break;
  }

  // If `observed_devices` is null, that means we are not yet in a valid chrome
  // session.
  if (!observed_devices) {
    return;
  }

  std::vector<std::string> previously_observed_devices =
      observed_devices->GetValue();

  if (!base::Contains(previously_observed_devices, device_key)) {
    previously_observed_devices.emplace_back(device_key);
    observed_devices->SetValue(previously_observed_devices);
  }
}

}  // namespace ash
