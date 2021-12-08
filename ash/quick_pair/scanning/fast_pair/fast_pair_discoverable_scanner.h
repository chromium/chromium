// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_

#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

struct Device;
class DeviceMetadata;
enum class PairFailure;

using DeviceCallback = base::RepeatingCallback<void(scoped_refptr<Device>)>;

// This class detects Fast Pair 'discoverable' advertisements (see
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenDiscoverable)
// and invokes the |found_callback| when it finds a device within the
// appropriate range.  |lost_callback| will be invoked when that device is lost
// to the bluetooth adapter.
class FastPairDiscoverableScanner final : public FastPairScanner::Observer {
 public:
  FastPairDiscoverableScanner(scoped_refptr<FastPairScanner> scanner,
                              scoped_refptr<device::BluetoothAdapter> adatper,
                              DeviceCallback found_callback,
                              DeviceCallback lost_callback);
  FastPairDiscoverableScanner(const FastPairDiscoverableScanner&) = delete;
  FastPairDiscoverableScanner& operator=(const FastPairDiscoverableScanner&) =
      delete;
  ~FastPairDiscoverableScanner() override;

  // FastPairScanner::Observer
  void OnDeviceFound(device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothDevice* device) override;

 private:
  void OnModelIdRetrieved(device::BluetoothDevice* device,
                          const absl::optional<std::string>& model_id);
  void OnDeviceMetadataRetrieved(device::BluetoothDevice* device,
                                 const std::string model_id,
                                 DeviceMetadata* device_metadata);
  void OnHandshakeComplete(scoped_refptr<Device> device,
                           absl::optional<PairFailure> failure);
  void NotifyDeviceFound(scoped_refptr<Device> device);
  void OnUtilityProcessStopped(
      device::BluetoothDevice* device,
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  scoped_refptr<FastPairScanner> scanner_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
  base::flat_map<std::string, scoped_refptr<Device>> notified_devices_;
  base::flat_map<std::string, int> model_id_parse_attempts_;
  base::ScopedObservation<FastPairScanner, FastPairScanner::Observer>
      observation_{this};
  base::WeakPtrFactory<FastPairDiscoverableScanner> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_
