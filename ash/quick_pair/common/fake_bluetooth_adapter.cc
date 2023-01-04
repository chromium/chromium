// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fake_bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

void FakeBluetoothAdapter::NotifyPoweredChanged(bool powered) {
  device::BluetoothAdapter::NotifyAdapterPoweredChanged(powered);
}

void FakeBluetoothAdapter::SetBluetoothIsPowered(bool powered) {
  is_bluetooth_powered_ = powered;
  NotifyPoweredChanged(powered);
}

void FakeBluetoothAdapter::SetBluetoothIsPresent(bool present) {
  is_bluetooth_present_ = present;
}

void FakeBluetoothAdapter::SetHardwareOffloadingStatus(
    device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
        hardware_offloading_status) {
  hardware_offloading_status_ = hardware_offloading_status;
  NotifyLowEnergyScanSessionHardwareOffloadingStatusChanged(
      hardware_offloading_status);
}

void FakeBluetoothAdapter::NotifyRemoved(device::BluetoothDevice* device) {
  for (auto& observer : observers_) {
    observer.DeviceRemoved(this, device);
  }
}

bool FakeBluetoothAdapter::IsPowered() const {
  return is_bluetooth_powered_;
}

bool FakeBluetoothAdapter::IsPresent() const {
  return is_bluetooth_present_;
}

device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
FakeBluetoothAdapter::GetLowEnergyScanSessionHardwareOffloadingStatus() {
  return hardware_offloading_status_;
}

device::BluetoothDevice* FakeBluetoothAdapter::GetDevice(
    const std::string& address) {
  for (const auto& it : mock_devices_) {
    if (it->GetAddress() == address) {
      return it.get();
    }
  }
  return nullptr;
}

}  // namespace quick_pair
}  // namespace ash
