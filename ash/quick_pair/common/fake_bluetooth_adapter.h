// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAKE_BLUETOOTH_ADAPTER_H_
#define ASH_QUICK_PAIR_COMMON_FAKE_BLUETOOTH_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:

  void SetBluetoothIsPowered(bool powered);

  void SetBluetoothIsPresent(bool present);

  void SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          hardware_offloading_status);

  void NotifyDeviceRemoved(device::BluetoothDevice* device);

  void SetConnectFailure() { connect_device_failure_ = true; }

  // This will force the next 'GetDevice()' call to return a nullptr. This is
  // used to test codepaths where the adapter is not able to return the device.
  void SetGetDeviceNullptr() { get_device_returns_nullptr_ = true; }

  // This will alter the 'ConnectDevice()' call to return before firing its
  // callback, mimicking a timeout situation.
  void SetConnectDeviceTimeout() { connect_device_timeout_ = true; }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service);

  void NotifyGattCharacteristicValueChanged(
      device::BluetoothRemoteGattCharacteristic* characteristic);

  void NotifyConfirmPasskey(uint32_t passkey, device::BluetoothDevice* device);
  void NotifyDisplayPasskey(device::BluetoothDevice* device, uint32_t passkey);

  void NotifyDevicePairedChanged(device::BluetoothDevice* device,
                                 bool new_paired_status);

  void NotifyDeviceChanged(device::BluetoothDevice* device);

  void NotifyDeviceConnectedStateChanged(device::BluetoothDevice* device,
                                         bool is_now_connected);
  void NotifyDeviceAdded(device::BluetoothDevice* device);

  bool IsPowered() const override;

  bool IsPresent() const override;

  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override;

  device::BluetoothDevice* GetDevice(const std::string& address) override;

  void AddPairingDelegate(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      PairingDelegatePriority priority) override;

  void ConnectDevice(
      const std::string& address,
      const std::optional<device::BluetoothDevice::AddressType>& address_type,
      base::OnceCallback<void(device::BluetoothDevice*)> callback,
      base::OnceCallback<void(const std::string&)> error_callback) override;

 private:
  ~FakeBluetoothAdapter() = default;

  bool is_bluetooth_powered_ = false;
  bool is_bluetooth_present_ = true;
  bool connect_device_failure_ = false;
  bool get_device_returns_nullptr_ = false;
  bool connect_device_timeout_ = false;
  raw_ptr<device::BluetoothDevice::PairingDelegate, DanglingUntriaged>
      pairing_delegate_ = nullptr;
  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      hardware_offloading_status_ = device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAKE_BLUETOOTH_ADAPTER_H_
