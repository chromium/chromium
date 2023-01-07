// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BLUETOOTH_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BLUETOOTH_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// Observes BluetoothAdapter::IsPowered state and exposes a callback to be
// notified on changes.
class BluetoothEnabledProvider : public BaseEnabledProvider,
                                 public device::BluetoothAdapter::Observer {
 public:
  BluetoothEnabledProvider();
  ~BluetoothEnabledProvider() override;

 private:
  // BluetoothAdapter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void LowEnergyScanSessionHardwareOffloadingStatusChanged(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          status) override;

  void OnAdapterReceived(scoped_refptr<device::BluetoothAdapter> adapter);

  void Update();

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothEnabledProvider> weak_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BLUETOOTH_ENABLED_PROVIDER_H_
