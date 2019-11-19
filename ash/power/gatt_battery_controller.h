// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_GATT_BATTERY_CONTROLLER_H_
#define ASH_POWER_GATT_BATTERY_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {

class GattBatteryPoller;

// Creates a GattBatteryPoller for each new connected Bluetooth device.
class ASH_EXPORT GattBatteryController
    : public device::BluetoothAdapter::Observer {
 public:
  GattBatteryController(scoped_refptr<device::BluetoothAdapter> adapter);
  ~GattBatteryController() override;

 private:
  friend class GattBatteryControllerTest;

  // device::BluetoothAdapter::Observer:
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // Creates a GattBatteryPoller for the device if one doesn't exist already.
  void EnsurePollerExistsForDevice(device::BluetoothDevice* device);

  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Map of Pollers indexed by the Bluetooth address of the connected device
  // they refer to.
  base::flat_map<std::string, std::unique_ptr<GattBatteryPoller>> poller_map_;

  DISALLOW_COPY_AND_ASSIGN(GattBatteryController);
};

}  // namespace ash

#endif  // ASH_POWER_GATT_BATTERY_CONTROLLER_H_
