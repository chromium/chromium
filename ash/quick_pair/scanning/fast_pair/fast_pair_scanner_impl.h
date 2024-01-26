// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {
namespace quick_pair {

// This registers a BluetoothLowEnergyScanner with the Advertisement Monitoring
// API and exposes the Fast Pair devices found/lost events to its observers.
class FastPairScannerImpl
    : public FastPairScanner,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  class Factory {
   public:
    static scoped_refptr<FastPairScanner> Create();

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual scoped_refptr<FastPairScanner> CreateInstance() = 0;

   private:
    static Factory* g_test_factory_;
  };

  // FastPairScanner::Observer
  void AddObserver(FastPairScanner::Observer* observer) override;
  void RemoveObserver(FastPairScanner::Observer* observer) override;
  void OnDevicePaired(scoped_refptr<Device> device) override;

  FastPairScannerImpl();
  FastPairScannerImpl(const FastPairScannerImpl&) = delete;
  FastPairScannerImpl& operator=(const FastPairScannerImpl&) = delete;

 private:
  ~FastPairScannerImpl() override;

  void StartScanning();

  // device::BluetoothAdapter::Observer
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // device::BluetoothLowEnergyScanSession::Delegate
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override;
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override;
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  void NotifyDeviceFound(device::BluetoothDevice* device);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Map of a Bluetooth device address to a set of advertisement data we have
  // seen.
  std::map<std::string, std::set<std::vector<uint8_t>>>
      device_address_advertisement_data_map_;

  base::flat_map<std::string, std::string> ble_address_to_classic_;

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
