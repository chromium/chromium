// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash {

// Fake BluetoothDeviceListController implementation.
class ASH_EXPORT FakeBluetoothDeviceListController
    : public BluetoothDeviceListController {
 public:
  FakeBluetoothDeviceListController();
  FakeBluetoothDeviceListController(const FakeBluetoothDeviceListController&) =
      delete;
  FakeBluetoothDeviceListController& operator=(
      const FakeBluetoothDeviceListController&) = delete;
  ~FakeBluetoothDeviceListController() override;

  size_t connected_devices_count() const { return connected_devices_count_; }

  size_t previously_connected_devices_count() const {
    return previously_connected_devices_count_;
  }

  const std::optional<bool>& last_bluetooth_enabled_state() const {
    return last_bluetooth_enabled_state_;
  }

 private:
  // BluetoothDeviceListController:
  void UpdateBluetoothEnabledState(bool enabled) override;
  void UpdateDeviceList(
      const PairedBluetoothDevicePropertiesPtrs& connected,
      const PairedBluetoothDevicePropertiesPtrs& previously_connected) override;

  size_t connected_devices_count_ = 0;
  size_t previously_connected_devices_count_ = 0;
  std::optional<bool> last_bluetooth_enabled_state_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_
