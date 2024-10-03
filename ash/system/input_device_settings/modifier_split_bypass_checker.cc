// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/modifier_split_bypass_checker.h"

#include "ash/constants/ash_features.h"
#include "ash/picker/picker_controller.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

ModifierSplitBypassChecker::ModifierSplitBypassChecker() {
  CHECK(features::IsModifierSplitEnabled());
  CHECK(base::FeatureList::IsEnabled(features::kModifierSplitDeviceEnabled));
  if (Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()) {
    return;
  }

  StartCheckingToEnableFeature();
}
ModifierSplitBypassChecker::~ModifierSplitBypassChecker() = default;

void ModifierSplitBypassChecker::OnInputDeviceConfigurationChanged(
    uint8_t input_device_type) {
  if (input_device_type & ui::InputDeviceEventObserver::kKeyboard) {
    CheckIfFeaturesShouldbeEnabled();
  }
}

void ModifierSplitBypassChecker::OnDeviceListsComplete() {
  CheckIfFeaturesShouldbeEnabled();
}

void ModifierSplitBypassChecker::StartCheckingToEnableFeature() {
  CheckIfFeaturesShouldbeEnabled();
  input_device_event_observation_.Observe(ui::DeviceDataManager::GetInstance());
}

void ModifierSplitBypassChecker::CheckIfFeaturesShouldbeEnabled() {
  const auto& keyboards =
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();
  for (const auto& keyboard : keyboards) {
    if (Shell::Get()->keyboard_capability()->IsSplitModifierKeyboardForOverride(
            keyboard)) {
      ForceEnableFeatures();
      break;
    }
  }
}

void ModifierSplitBypassChecker::ForceEnableFeatures() {
  Shell::Get()->keyboard_capability()->ForceEnableFeature();
  Shell::Get()
      ->input_device_settings_controller()
      ->ForceKeyboardSettingRefreshWhenFeatureEnabled();

  // Reset observing as we are no longer interested in seeing when new keyboards
  // connect.
  input_device_event_observation_.Reset();
}

}  // namespace ash
