// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_BATTERY_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_BATTERY_VIEW_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// This class encapsulates the logic of configuring the view shown for a single
// Bluetooth device battery for a device in the detailed Bluetooth page within
// the quick settings.
class ASH_EXPORT BluetoothDeviceListItemBatteryView : public views::View {
  METADATA_HEADER(BluetoothDeviceListItemBatteryView, views::View)

 public:
  BluetoothDeviceListItemBatteryView();
  BluetoothDeviceListItemBatteryView(
      const BluetoothDeviceListItemBatteryView&) = delete;
  BluetoothDeviceListItemBatteryView& operator=(
      const BluetoothDeviceListItemBatteryView&) = delete;
  ~BluetoothDeviceListItemBatteryView() override;

  // Update the battery icon and text to reflect |new_battery_percentage|, and
  // the label will be set to |label_string_id|.
  void UpdateBatteryInfo(const uint8_t new_battery_percentage,
                         const int label_string_id);

 private:
  // Evaluates whether the |old_charge_percent| and |new_charge_percent| values
  // are different enough to warrant updating the view. We avoid updating the
  // view if possible since the battery icon is not cached.
  bool ApproximatelyEqual(uint8_t old_charge_percent,
                          uint8_t new_charge_percent) const;

  std::optional<uint8_t> last_shown_battery_percentage_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_ITEM_BATTERY_VIEW_H_
