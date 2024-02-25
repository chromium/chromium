// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_NOT_DISCOVERABLE_SCANNER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_NOT_DISCOVERABLE_SCANNER_IMPL_H_

#include <memory>
#include <sstream>
#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

class AccountKeyFilter;
struct NotDiscoverableAdvertisement;
enum class PairFailure;
struct PairingMetadata;

// This class detects Fast Pair 'not discoverable' advertisements (see
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenNotDiscoverable)
// and invokes the |found_callback| when it finds a device within the
// appropriate range.  |lost_callback| will be invoked when that device is lost
// to the bluetooth adapter.
class FastPairNotDiscoverableScannerImpl
    : public FastPairNotDiscoverableScanner,
      public FastPairScanner::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairNotDiscoverableScanner> Create(
        scoped_refptr<FastPairScanner> scanner,
        scoped_refptr<device::BluetoothAdapter> adapter,
        DeviceCallback found_callback,
        DeviceCallback lost_callback);

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairNotDiscoverableScanner> CreateInstance(
        scoped_refptr<FastPairScanner> scanner,
        scoped_refptr<device::BluetoothAdapter> adapter,
        DeviceCallback found_callback,
        DeviceCallback lost_callback) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairNotDiscoverableScannerImpl(
      scoped_refptr<FastPairScanner> scanner,
      scoped_refptr<device::BluetoothAdapter> adatper,
      DeviceCallback found_callback,
      DeviceCallback lost_callback);
  FastPairNotDiscoverableScannerImpl(
      const FastPairNotDiscoverableScannerImpl&) = delete;
  FastPairNotDiscoverableScannerImpl& operator=(
      const FastPairNotDiscoverableScannerImpl&) = delete;
  ~FastPairNotDiscoverableScannerImpl() override;

  // FastPairScanner::Observer
  void OnDeviceFound(device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothDevice* device) override;

 private:
  void OnAdvertisementParsed(
      const std::string& address,
      const std::optional<NotDiscoverableAdvertisement>& advertisement);
  void OnAccountKeyFilterCheckResult(const std::string& address,
                                     std::optional<PairingMetadata> metadata);
  void NotifyDeviceFound(scoped_refptr<Device> device);
  void OnUtilityProcessStopped(
      const std::string& address,
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  scoped_refptr<FastPairScanner> scanner_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
  base::flat_map<std::string, scoped_refptr<Device>> notified_devices_;
  base::flat_map<std::string, int> advertisement_parse_attempts_;
  base::flat_map<std::string, AccountKeyFilter> account_key_filters_;
  base::ScopedObservation<FastPairScanner, FastPairScanner::Observer>
      observation_{this};
  base::WeakPtrFactory<FastPairNotDiscoverableScannerImpl>
      weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_NOT_DISCOVERABLE_SCANNER_IMPL_H_
