// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_LEGACY_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_LEGACY_H_

#include <string>

#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button of bluetooth.
// TODO(crbug.com/1234138): Remove this class when the
// |ash::features::kBluetoothRevamp| feature flag is fully launched.
class BluetoothFeaturePodControllerLegacy
    : public FeaturePodControllerBase,
      public TrayBluetoothHelper::Observer {
 public:
  BluetoothFeaturePodControllerLegacy(
      UnifiedSystemTrayController* tray_controller);

  BluetoothFeaturePodControllerLegacy(
      const BluetoothFeaturePodControllerLegacy&) = delete;
  BluetoothFeaturePodControllerLegacy& operator=(
      const BluetoothFeaturePodControllerLegacy&) = delete;

  ~BluetoothFeaturePodControllerLegacy() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void UpdateButton();
  void SetTooltipState(const std::u16string& tooltip_state);

  // BluetoothObserver:
  void OnBluetoothSystemStateChanged() override;
  void OnBluetoothScanStateChanged() override;
  void OnBluetoothDeviceListChanged() override;

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_LEGACY_H_
