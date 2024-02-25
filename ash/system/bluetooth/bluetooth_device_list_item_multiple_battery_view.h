// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_MULTIPLE_BATTERY_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_MULTIPLE_BATTERY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash {

// This class encapsulates the logic of configuring the view shown for multiple
// batteries (left bud, case, right bud) of a single device.
class ASH_EXPORT BluetoothDeviceListItemMultipleBatteryView
    : public views::View {
  METADATA_HEADER(BluetoothDeviceListItemMultipleBatteryView, views::View)

 public:
  BluetoothDeviceListItemMultipleBatteryView();
  BluetoothDeviceListItemMultipleBatteryView(
      const BluetoothDeviceListItemMultipleBatteryView&) = delete;
  BluetoothDeviceListItemMultipleBatteryView& operator=(
      const BluetoothDeviceListItemMultipleBatteryView&) = delete;
  ~BluetoothDeviceListItemMultipleBatteryView() override;

  // Update the battery icon and text to reflect |battery_properties|.
  void UpdateBatteryInfo(
      const bluetooth_config::mojom::DeviceBatteryInfoPtr& battery_info);

 private:
  raw_ptr<BluetoothDeviceListItemBatteryView, DanglingUntriaged>
      left_bud_battery_view_ = nullptr;
  raw_ptr<BluetoothDeviceListItemBatteryView> case_battery_view_ = nullptr;
  raw_ptr<BluetoothDeviceListItemBatteryView> right_bud_battery_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_MULTIPLE_BATTERY_VIEW_H_
