// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_MODIFIER_SPLIT_BYPASS_CHECKER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_MODIFIER_SPLIT_BYPASS_CHECKER_H_

#include "base/scoped_observation.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

class ModifierSplitBypassChecker : public ui::InputDeviceEventObserver {
 public:
  ModifierSplitBypassChecker();
  ModifierSplitBypassChecker(const ModifierSplitBypassChecker&) = delete;
  ModifierSplitBypassChecker& operator=(const ModifierSplitBypassChecker&) =
      delete;
  ~ModifierSplitBypassChecker() override;

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_type) override;
  void OnDeviceListsComplete() override;

 private:
  void StartCheckingToEnableFeature();
  void CheckIfFeaturesShouldbeEnabled();
  void ForceEnableFeatures();

  base::ScopedObservation<ui::DeviceDataManager, ui::InputDeviceEventObserver>
      input_device_event_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_MODIFIER_SPLIT_BYPASS_CHECKER_H_
