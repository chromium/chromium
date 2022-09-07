// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/timer/timer.h"

namespace ash {

class BluetoothDetailedViewLegacy;
class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Bluetooth detailed view in UnifiedSystemTray.
class ASH_EXPORT UnifiedBluetoothDetailedViewController
    : public DetailedViewController,
      public TrayBluetoothHelper::Observer {
 public:
  explicit UnifiedBluetoothDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedBluetoothDetailedViewController(
      const UnifiedBluetoothDetailedViewController&) = delete;
  UnifiedBluetoothDetailedViewController& operator=(
      const UnifiedBluetoothDetailedViewController&) = delete;

  ~UnifiedBluetoothDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  std::u16string GetAccessibleName() const override;

  // BluetoothObserver:
  void OnBluetoothSystemStateChanged() override;
  void OnBluetoothScanStateChanged() override;
  void OnBluetoothDeviceListChanged() override;

 private:
  void UpdateDeviceListAndUI();
  void UpdateBluetoothDeviceList();

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  BluetoothDetailedViewLegacy* view_ = nullptr;

  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
