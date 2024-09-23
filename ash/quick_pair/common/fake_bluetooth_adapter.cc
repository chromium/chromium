// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fake_bluetooth_adapter.h"

namespace {

const std::vector<uint8_t>& kTestWriteResponse{0x01, 0x03, 0x02, 0x01, 0x02};

}  // namespace

namespace ash::quick_pair {

void FakeBluetoothAdapter::SetBluetoothIsPowered(bool powered) {
  is_bluetooth_powered_ = powered;
  device::BluetoothAdapter::NotifyAdapterPoweredChanged(powered);
}

void FakeBluetoothAdapter::SetBluetoothIsPresent(bool present) {
  is_bluetooth_present_ = present;
  device::BluetoothAdapter::NotifyAdapterPresentChanged(present);
}

void FakeBluetoothAdapter::SetHardwareOffloadingStatus(
    device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
        hardware_offloading_status) {
  hardware_offloading_status_ = hardware_offloading_status;
  NotifyLowEnergyScanSessionHardwareOffloadingStatusChanged(
      hardware_offloading_status);
}

void FakeBluetoothAdapter::NotifyDeviceRemoved(
    device::BluetoothDevice* device) {
  for (auto& observer : observers_) {
    observer.DeviceRemoved(this, device);
  }
}

void FakeBluetoothAdapter::NotifyGattDiscoveryCompleteForService(
    device::BluetoothRemoteGattService* service) {
  device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
}

void FakeBluetoothAdapter::NotifyGattCharacteristicValueChanged(
    device::BluetoothRemoteGattCharacteristic* characteristic) {
  device::BluetoothAdapter::NotifyGattCharacteristicValueChanged(
      characteristic, kTestWriteResponse);
}

void FakeBluetoothAdapter::NotifyConfirmPasskey(
    uint32_t passkey,
    device::BluetoothDevice* device) {
  pairing_delegate_->ConfirmPasskey(device, passkey);
}

void FakeBluetoothAdapter::NotifyDisplayPasskey(device::BluetoothDevice* device,
                                                uint32_t passkey) {
  pairing_delegate_->DisplayPasskey(device, passkey);
}

void FakeBluetoothAdapter::NotifyDevicePairedChanged(
    device::BluetoothDevice* device,
    bool new_paired_status) {
  for (auto& observer : GetObservers()) {
    observer.DevicePairedChanged(this, device, new_paired_status);
  }
}

void FakeBluetoothAdapter::NotifyDeviceChanged(
    device::BluetoothDevice* device) {
  for (auto& observer : GetObservers()) {
    observer.DeviceChanged(this, device);
  }
}

void FakeBluetoothAdapter::NotifyDeviceConnectedStateChanged(
    device::BluetoothDevice* device,
    bool is_now_connected) {
  for (auto& observer : observers_) {
    observer.DeviceConnectedStateChanged(this, device, is_now_connected);
  }
}

void FakeBluetoothAdapter::NotifyDeviceAdded(device::BluetoothDevice* device) {
  for (auto& observer : observers_) {
    observer.DeviceAdded(this, device);
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
  // There are a few situations where we want GetDevice to return nullptr. For
  // example, if we want the Pairer to "pair by address" then GetDevice should
  // return nullptr when called on the mac address.
  if (get_device_returns_nullptr_) {
    get_device_returns_nullptr_ = false;
    return nullptr;
  }

  for (const auto& it : mock_devices_) {
    if (it->GetAddress() == address) {
      return it.get();
    }
  }

  return nullptr;
}

void FakeBluetoothAdapter::AddPairingDelegate(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    PairingDelegatePriority priority) {
  pairing_delegate_ = pairing_delegate;
}

void FakeBluetoothAdapter::ConnectDevice(
    const std::string& address,
    const std::optional<device::BluetoothDevice::AddressType>& address_type,
    base::OnceCallback<void(device::BluetoothDevice*)> callback,
    base::OnceCallback<void(const std::string&)> error_callback) {
  if (connect_device_failure_) {
    std::move(error_callback).Run(std::string());
    return;
  }

  // If |connect_device_timeout_| is set, mimic a timeout by returning before
  // calling the success callback.
  if (connect_device_timeout_) {
    return;
  }

  std::move(callback).Run(GetDevice(address));
}

}  // namespace ash::quick_pair
