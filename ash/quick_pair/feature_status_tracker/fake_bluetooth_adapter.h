// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_

#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  void NotifyPoweredChanged(bool powered);

  void SetBluetoothIsPowered(bool powered);

  void SetBluetoothIsPresent(bool present);

  void SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          hardware_offloading_status);

  bool IsPowered() const override;

  bool IsPresent() const override;

  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override;

  device::BluetoothDevice* GetDevice(const std::string& address) override;

 private:
  ~FakeBluetoothAdapter() = default;

  bool is_bluetooth_powered_ = false;
  bool is_bluetooth_present_ = true;
  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      hardware_offloading_status_ = device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_
