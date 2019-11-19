// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_controller.h"

#include "ash/power/gatt_battery_poller.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {

GattBatteryController::GattBatteryController(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {
  adapter_->AddObserver(this);
}

GattBatteryController::~GattBatteryController() {
  adapter_->RemoveObserver(this);
}

void GattBatteryController::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  if (is_now_connected) {
    EnsurePollerExistsForDevice(device);
    return;
  }

  poller_map_.erase(device->GetAddress());
}

void GattBatteryController::DeviceAdded(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  if (device->IsConnected())
    EnsurePollerExistsForDevice(device);
}

void GattBatteryController::DeviceRemoved(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  poller_map_.erase(device->GetAddress());
}

void GattBatteryController::EnsurePollerExistsForDevice(
    device::BluetoothDevice* device) {
  const std::string device_address = device->GetAddress();

  // Don't do anything if the device is already registered.
  if (base::Contains(poller_map_, device_address))
    return;

  poller_map_[device_address] = GattBatteryPoller::Factory::NewInstance(
      adapter_, device_address, std::make_unique<base::OneShotTimer>());
}

}  // namespace ash
