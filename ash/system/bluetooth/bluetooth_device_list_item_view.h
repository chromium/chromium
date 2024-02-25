// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ViewClickListener;

// This class encapsulates the logic of configuring the view shown for a single
// device in the detailed Bluetooth page within the quick settings.
class ASH_EXPORT BluetoothDeviceListItemView : public HoverHighlightView {
  METADATA_HEADER(BluetoothDeviceListItemView, HoverHighlightView)

 public:
  explicit BluetoothDeviceListItemView(ViewClickListener* listener);
  BluetoothDeviceListItemView(const BluetoothDeviceListItemView&) = delete;
  BluetoothDeviceListItemView& operator=(const BluetoothDeviceListItemView&) =
      delete;
  ~BluetoothDeviceListItemView() override;

  // Update the view to reflect the latest position of this device within the
  // list of devices, e.g. with |device_index| and |total_device_count|, and to
  // reflect the given device properties |device_properties|.
  void UpdateDeviceProperties(
      size_t device_index,
      size_t total_device_count,
      const bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
          device_properties);

  const bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
  device_properties() const {
    return device_properties_;
  }

 private:
  // Updates the a11y name used for this view. This name should include the name
  // of the device, the type of the device, the connected state of the device,
  // any battery information available, and the index of the device within the
  // device list.
  void UpdateAccessibleName(size_t device_index, size_t total_device_count);

  // Update the view responsible for showing the battery percentage to reflect
  // the given battery information |battery_info|.
  void UpdateBatteryInfo(
      const bluetooth_config::mojom::DeviceBatteryInfoPtr& battery_info);

  void UpdateSingleBatteryView(
      const bluetooth_config::mojom::DeviceBatteryInfoPtr& battery_info);

  void UpdateMultipleBatteryView(
      const bluetooth_config::mojom::DeviceBatteryInfoPtr& battery_info);

  bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr
      device_properties_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_VIEW_H_
