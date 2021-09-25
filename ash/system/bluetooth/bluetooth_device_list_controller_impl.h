// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash {
namespace tray {
class BluetoothDetailedView;
}  // namespace tray

class TriView;

// BluetoothDeviceListController implementation.
class ASH_EXPORT BluetoothDeviceListControllerImpl
    : public BluetoothDeviceListController {
 public:
  explicit BluetoothDeviceListControllerImpl(
      tray::BluetoothDetailedView* bluetooth_detailed_view);
  BluetoothDeviceListControllerImpl(const BluetoothDeviceListControllerImpl&) =
      delete;
  BluetoothDeviceListControllerImpl& operator=(
      const BluetoothDeviceListControllerImpl&) = delete;
  ~BluetoothDeviceListControllerImpl() override = default;

 private:
  // BluetoothDeviceListController:
  void UpdateBluetoothEnabledState(bool enabled) override;
  void UpdateDeviceList(
      const PairedBluetoothDevicePropertiesPtrs& connected,
      const PairedBluetoothDevicePropertiesPtrs& previously_connected) override;

  // Adds a new sub-header with |text_id| if |sub_header| is |nullptr|,
  // otherwise reuses |sub_header|. Whichever sub-header used is then reordered
  // to |index| and returned.
  TriView* AddOrReorderSubHeader(TriView* sub_header, int text_id, int index);

  tray::BluetoothDetailedView* bluetooth_detailed_view_;

  bool is_bluetooth_enabled_ = false;
  TriView* currently_connected_devices_sub_header_ = nullptr;
  TriView* previously_connected_devices_sub_header_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_
