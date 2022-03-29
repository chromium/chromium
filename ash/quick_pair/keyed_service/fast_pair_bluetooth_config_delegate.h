// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {
class AdapterStateController;
class DeviceImageInfo;
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
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAdapterStateControllerChanged(
        chromeos::bluetooth_config::AdapterStateController*
            adapter_state_controller) = 0;
  };

  FastPairBluetoothConfigDelegate();
  FastPairBluetoothConfigDelegate(const FastPairBluetoothConfigDelegate&) =
      delete;
  FastPairBluetoothConfigDelegate& operator=(
      const FastPairBluetoothConfigDelegate&) = delete;
  ~FastPairBluetoothConfigDelegate() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // chromeos::bluetooth_config::FastPairDelegate
  absl::optional<chromeos::bluetooth_config::DeviceImageInfo>
  GetDeviceImageInfo(const std::string& device_id) override;
  void SetAdapterStateController(
      chromeos::bluetooth_config::AdapterStateController*
          adapter_state_controller) override;
  void SetDeviceNameManager(chromeos::bluetooth_config::DeviceNameManager*
                                device_name_manager) override;

  chromeos::bluetooth_config::AdapterStateController*
  adapter_state_controller() {
    return adapter_state_controller_;
  }

 private:
  base::ObserverList<Observer> observers_;
  chromeos::bluetooth_config::AdapterStateController*
      adapter_state_controller_ = nullptr;
  chromeos::bluetooth_config::DeviceNameManager* device_name_manager_ = nullptr;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
