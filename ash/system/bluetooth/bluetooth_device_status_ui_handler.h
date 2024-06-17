// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_STATUS_UI_HANDLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_STATUS_UI_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Listens for changes in Bluetooth device status, like a when a new device is
// paired, a device is disconnected or connected. It shows a toast when these
// events occur.
class ASH_EXPORT BluetoothDeviceStatusUiHandler
    : public bluetooth_config::mojom::BluetoothDeviceStatusObserver {
 public:
  explicit BluetoothDeviceStatusUiHandler(PrefService* local_state);
  BluetoothDeviceStatusUiHandler(const BluetoothDeviceStatusUiHandler&) =
      delete;
  BluetoothDeviceStatusUiHandler& operator=(
      const BluetoothDeviceStatusUiHandler&) = delete;
  ~BluetoothDeviceStatusUiHandler() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // bluetooth_config::mojom::BluetoothDeviceStatusObserver:
  void OnDevicePaired(
      bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr device)
      override;
  void OnDeviceConnected(
      bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr device)
      override;
  void OnDeviceDisconnected(
      bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr device)
      override;

  virtual void ShowToast(ash::ToastData toast_data);

  // Returns a string which represents a toast id. Id is created from a
  // constant string prefix concatenated to |paired_device_properties| id.
  std::string GetToastId(
      const bluetooth_config::mojom::PairedBluetoothDeviceProperties*
          paired_device_properties);

  void BindToCrosBluetoothConfig();

  std::optional<base::TimeTicks> last_connection_timestamp_;

  raw_ptr<PrefService> local_state_;  // unowned.

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::BluetoothDeviceStatusObserver>
      cros_bluetooth_device_status_observer_receiver_{this};

  base::WeakPtrFactory<BluetoothDeviceStatusUiHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_STATUS_UI_HANDLER_H_
