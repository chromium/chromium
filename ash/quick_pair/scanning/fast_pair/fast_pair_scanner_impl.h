// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

// This registers a BluetoothLowEnergyScanner with the Advertisement Monitoring
// API and exposes the Fast Pair devices found/lost events to its observers.
class FastPairScannerImpl : public FastPairScanner,
                            public device::BluetoothAdapter::Observer {
 public:
  FastPairScannerImpl();
  ~FastPairScannerImpl() override;
  FastPairScannerImpl(const FastPairScanner&) = delete;
  FastPairScannerImpl& operator=(const FastPairScanner&) = delete;

  // FastPairScanner::Observer
  void AddObserver(FastPairScanner::Observer* observer) override;
  void RemoveObserver(FastPairScanner::Observer* observer) override;

 private:
  // device::BluetoothAdapter::Observer
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::ObserverList<FastPairScanner::Observer> observers_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<FastPairScannerImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_
