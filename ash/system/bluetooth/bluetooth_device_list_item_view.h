// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash {

class ViewClickListener;

// This class encapsulates the logic of configuring the view shown for a single
// device in the detailed Bluetooth page within the quick settings.
class ASH_EXPORT BluetoothDeviceListItemView : public HoverHighlightView {
 public:
  explicit BluetoothDeviceListItemView(ViewClickListener* listener);
  BluetoothDeviceListItemView(const BluetoothDeviceListItemView&) = delete;
  BluetoothDeviceListItemView& operator=(const BluetoothDeviceListItemView&) =
      delete;
  ~BluetoothDeviceListItemView() override;

  // Update the view to reflect the given device properties |device_properties|.
  void UpdateDeviceProperties(
      const chromeos::bluetooth_config::mojom::
          PairedBluetoothDevicePropertiesPtr& device_properties);

  const chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
  device_properties() const {
    return device_properties_;
  }

 private:
  // views::View:
  const char* GetClassName() const override;

  // Update the view responsible for showing the battery percentage to reflect
  // the given battery information |battery_info|.
  void UpdateBatteryInfo(
      const chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr&
          battery_info);

  void UpdateSingleBatteryView(
      const chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr&
          battery_info);

  void UpdateMultipleBatteryView(
      const chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr&
          battery_info);

  chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr
      device_properties_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_
