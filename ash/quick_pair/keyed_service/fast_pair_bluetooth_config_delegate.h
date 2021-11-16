// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_

#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"

namespace chromeos {
namespace bluetooth_config {
class DeviceNameManager;
}  // namespace bluetooth_config
}  // namespace chromeos

namespace ash {
namespace quick_pair {

// Delegate class which provides Fast Pair information to the
// CrosBluetoothConfig system.
class FastPairBluetoothConfigDelegate
    : public chromeos::bluetooth_config::FastPairDelegate {
 public:
  FastPairBluetoothConfigDelegate();
  FastPairBluetoothConfigDelegate(const FastPairBluetoothConfigDelegate&) =
      delete;
  FastPairBluetoothConfigDelegate& operator=(
      const FastPairBluetoothConfigDelegate&) = delete;
  ~FastPairBluetoothConfigDelegate() override;

  // chromeos::bluetooth_config::FastPairDelegate
  void SetDeviceNameManager(chromeos::bluetooth_config::DeviceNameManager*
                                device_name_manager) override;

 private:
  chromeos::bluetooth_config::DeviceNameManager* device_name_manager_ = nullptr;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
