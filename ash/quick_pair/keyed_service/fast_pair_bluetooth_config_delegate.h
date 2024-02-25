// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"

namespace ash::bluetooth_config {
class AdapterStateController;
class DeviceImageInfo;
class DeviceNameManager;
}  // namespace ash::bluetooth_config

namespace ash {
namespace quick_pair {

// Delegate class which provides Fast Pair information to the
// CrosBluetoothConfig system.
class FastPairBluetoothConfigDelegate
    : public bluetooth_config::FastPairDelegate {
 public:
  // The delegate_ is set in the constructor and implements the following
  // virtual methods.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the AdapterStateController is changed.
    virtual void OnAdapterStateControllerChanged(
        bluetooth_config::AdapterStateController* adapter_state_controller) = 0;
  };

  explicit FastPairBluetoothConfigDelegate(Delegate* delegate);

  FastPairBluetoothConfigDelegate();
  FastPairBluetoothConfigDelegate(const FastPairBluetoothConfigDelegate&) =
      delete;
  FastPairBluetoothConfigDelegate& operator=(
      const FastPairBluetoothConfigDelegate&) = delete;
  ~FastPairBluetoothConfigDelegate() override;

  // bluetooth_config::FastPairDelegate
  std::optional<bluetooth_config::DeviceImageInfo> GetDeviceImageInfo(
      const std::string& device_id) override;
  void ForgetDevice(const std::string& mac_address) override;
  void UpdateDeviceNickname(const std::string& mac_address,
                            const std::string& nickname) override;
  void SetAdapterStateController(bluetooth_config::AdapterStateController*
                                     adapter_state_controller) override;
  void SetDeviceNameManager(
      bluetooth_config::DeviceNameManager* device_name_manager) override;

  bluetooth_config::AdapterStateController* adapter_state_controller() {
    return adapter_state_controller_;
  }

  // FastPairDelegate
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDeviceProperties() override;

  // Adds |device| to the list of Fast Pairable devices, assumes caller has
  // checked that |device| is eligible for Subsequent Pair. Notifies observers
  // that the list of Fast Pairable devices has changed.
  void AddFastPairDevice(scoped_refptr<Device> device);

  // Removes |device| from the list of Fast Pairable devices and notifies
  // observers that the list of Fast Pairable devices has changed.
  void RemoveFastPairDevice(scoped_refptr<Device> device);

  // Updates the pairing state of the corresponding device in the list of Fast
  // Pairable devices.
  void UpdateFastPairableDevicePairingState(
      scoped_refptr<Device> device,
      bluetooth_config::mojom::FastPairableDevicePairingState pairing_state);

  // Removes all devices from the list of Fast Pairable devices and notifies
  // observers that the list of Fast Pairable devices has changed.
  void ClearFastPairableDevices();

 private:
  bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr
  ConvertDeviceToProperties(
      scoped_refptr<Device> device,
      bluetooth_config::mojom::FastPairableDevicePairingState pairing_state);

  // List of Fast Pairable devices detected for the Subsequent Pairing flow.
  // These must be Device pointers, since this is what is expected to be
  // passed to the Pairer Broker.
  // TODO(b/293635165): Combine with |fast_pairable_device_properties_| into one
  // list.
  std::vector<scoped_refptr<Device>> fast_pairable_devices_;

  // List of Fast Pairable device properties. Expected to stay synced with
  // fast_pairable_device.
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_device_properties_;

  raw_ptr<Delegate> delegate_;
  raw_ptr<bluetooth_config::AdapterStateController> adapter_state_controller_ =
      nullptr;
  raw_ptr<bluetooth_config::DeviceNameManager> device_name_manager_ = nullptr;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_FAST_PAIR_BLUETOOTH_CONFIG_DELEGATE_H_
