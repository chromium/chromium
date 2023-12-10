// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_

#include <optional>
#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

class DeviceMetadata;
enum class PairFailure;

class FastPairDiscoverableScannerImpl : public FastPairDiscoverableScanner,
                                        public FastPairScanner::Observer,
                                        public NetworkStateHandlerObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairDiscoverableScanner> Create(
        scoped_refptr<FastPairScanner> scanner,
        scoped_refptr<device::BluetoothAdapter> adapter,
        DeviceCallback found_callback,
        DeviceCallback lost_callback);

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairDiscoverableScanner> CreateInstance(
        scoped_refptr<FastPairScanner> scanner,
        scoped_refptr<device::BluetoothAdapter> adapter,
        DeviceCallback found_callback,
        DeviceCallback lost_callback) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairDiscoverableScannerImpl(
      scoped_refptr<FastPairScanner> scanner,
      scoped_refptr<device::BluetoothAdapter> adapter,
      DeviceCallback found_callback,
      DeviceCallback lost_callback);
  FastPairDiscoverableScannerImpl(const FastPairDiscoverableScannerImpl&) =
      delete;
  FastPairDiscoverableScannerImpl& operator=(
      const FastPairDiscoverableScannerImpl&) = delete;
  ~FastPairDiscoverableScannerImpl() override;

  // FastPairScanner::Observer
  void OnDeviceFound(device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothDevice* device) override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

 private:
  void OnModelIdRetrieved(const std::string& address,
                          const std::optional<std::string>& model_id);
  void OnDeviceMetadataRetrieved(const std::string& address,
                                 const std::string model_id,
                                 DeviceMetadata* device_metadata,
                                 bool has_retryable_error);
  void NotifyDeviceFound(scoped_refptr<Device> device);
  void OnUtilityProcessStopped(
      const std::string& address,
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  scoped_refptr<FastPairScanner> scanner_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
  base::flat_map<std::string, std::string> pending_devices_address_to_model_id_;
  base::flat_map<std::string, scoped_refptr<Device>> notified_devices_;
  base::flat_map<std::string, int> model_id_parse_attempts_;
  base::ScopedObservation<FastPairScanner, FastPairScanner::Observer>
      observation_{this};
  base::WeakPtrFactory<FastPairDiscoverableScannerImpl> weak_pointer_factory_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_
