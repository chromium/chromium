// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker_impl.h"

#include <memory>
#include <string>

#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

InputDeviceTrackerImpl::InputDeviceTrackerImpl() = default;
InputDeviceTrackerImpl::~InputDeviceTrackerImpl() = default;

void InputDeviceTrackerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterListPref(prefs::kMouseObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kKeyboardObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kTouchpadObservedDevicesPref);
  pref_registry->RegisterListPref(prefs::kPointingStickObservedDevicesPref);
}

void InputDeviceTrackerImpl::ResetPrefMembers() {
  mouse_observed_devices_ = std::make_unique<StringListPrefMember>();
  touchpad_observed_devices_ = std::make_unique<StringListPrefMember>();
  keyboard_observed_devices_ = std::make_unique<StringListPrefMember>();
  pointing_stick_observed_devices_ = std::make_unique<StringListPrefMember>();
}

void InputDeviceTrackerImpl::Init(PrefService* pref_service) {
  ResetPrefMembers();

  mouse_observed_devices_->Init(prefs::kMouseObservedDevicesPref, pref_service);
  touchpad_observed_devices_->Init(prefs::kTouchpadObservedDevicesPref,
                                   pref_service);
  keyboard_observed_devices_->Init(prefs::kKeyboardObservedDevicesPref,
                                   pref_service);
  pointing_stick_observed_devices_->Init(
      prefs::kPointingStickObservedDevicesPref, pref_service);
}

void InputDeviceTrackerImpl::RecordDeviceConnected(
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
  DCHECK(observed_devices);

  std::vector<std::string> previously_observed_devices =
      observed_devices->GetValue();

  if (!base::Contains(previously_observed_devices, device_key)) {
    previously_observed_devices.emplace_back(device_key);
    observed_devices->SetValue(previously_observed_devices);
  }
}

}  // namespace ash
