// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_STATE_CACHE_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_STATE_CACHE_H_

#include "ash/ash_export.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Queries the Bluetooth adapter state on startup, then monitors it for changes.
// Used by code that needs to query the adapter state synchronously (e.g. the
// quick settings bubble).
class ASH_EXPORT BluetoothStateCache
    : public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  BluetoothStateCache();
  BluetoothStateCache(const BluetoothStateCache&) = delete;
  BluetoothStateCache& operator=(const BluetoothStateCache&) = delete;
  ~BluetoothStateCache() override;

  // Returns the cached system state.
  bluetooth_config::mojom::BluetoothSystemState system_state() {
    return system_state_;
  }

 private:
  // Binds to the mojo interface for bluetooth config.
  void BindToCrosBluetoothConfig();

  // bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      system_properties_observer_receiver_{this};

  // Most users have bluetooth enabled, so use that as the default until the
  // initial value is available.
  bluetooth_config::mojom::BluetoothSystemState system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kEnabled;

  base::WeakPtrFactory<BluetoothStateCache> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_STATE_CACHE_H_
