// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"

namespace ash {
namespace quick_pair {

// This registers a BluetoothLowEnergyScanner with the Advertisement Monitoring
// API and exposes the Fast Pair devices found/lost events to its observers.
class FastPairScannerImpl
    : public FastPairScanner,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  FastPairScannerImpl();
  FastPairScannerImpl(const FastPairScannerImpl&) = delete;
  FastPairScannerImpl& operator=(const FastPairScannerImpl&) = delete;

  // FastPairScanner::Observer
  void AddObserver(FastPairScanner::Observer* observer) override;
  void RemoveObserver(FastPairScanner::Observer* observer) override;

 private:
  ~FastPairScannerImpl() override;

  // device::BluetoothAdapter::Observer
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // device::BluetoothLowEnergyScanSession::Delegate
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override;
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      absl::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override;
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  void NotifyDeviceFound(device::BluetoothDevice* device);

  // Map of a Bluetooth device address to a set of advertisement data we have
  // seen.
  std::map<std::string, std::set<std::vector<uint8_t>>>
      device_address_advertisement_data_map_;

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
      background_scan_session_;
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
