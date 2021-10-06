// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_

#include <string>

#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

struct Device;
struct NotDiscoverableAdvertisement;
struct PairingMetadata;
class DeviceMetadata;

class RetroactivePairingDetectorImpl final
    : public RetroactivePairingDetector,
      public device::BluetoothAdapter::Observer,
      public PairerBroker::Observer {
 public:
  explicit RetroactivePairingDetectorImpl(PairerBroker* pairer_broker);
  RetroactivePairingDetectorImpl(const RetroactivePairingDetectorImpl&) =
      delete;
  RetroactivePairingDetectorImpl& operator=(
      const RetroactivePairingDetectorImpl&) = delete;
  ~RetroactivePairingDetectorImpl() override;

  // RetroactivePairingDetector:
  void AddObserver(RetroactivePairingDetector::Observer* observer) override;
  void RemoveObserver(RetroactivePairingDetector::Observer* observer) override;

 private:
  // device::BluetoothAdapter::Observer
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // PairerBroker::Observer
  void OnDevicePaired(scoped_refptr<Device> device) override;
  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override;
  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         absl::optional<AccountKeyFailure> error) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Converts a Bluetooth device to a Fast Pair Device and notifies observers
  // that a device has been found to retroactively pair to.
  void NotifyDeviceFound(const std::string& model_id,
                         const std::string& device_address);

  void OnModelIdRetrieved(const std::string& device_address,
                          const absl::optional<std::string>& model_id);
  void OnDeviceMetadataRetrieved(const std::string& device_address,
                                 const std::string model_id,
                                 DeviceMetadata* device_metadata);
  void OnUtilityProcessStoppedOnGetHexModelId(
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  void CheckAdvertisementData(const std::string& device_address);
  void OnAdvertisementParsed(
      const std::string& device_address,
      const absl::optional<NotDiscoverableAdvertisement>& advertisement);
  void OnAccountKeyFilterCheckResult(const std::string& device_address,
                                     absl::optional<PairingMetadata> metadata);
  void OnUtilityProcessStoppedOnParseAdvertisement(
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  // The Bluetooth pairing addresses of Fast Pair devices that we have already
  // paired to.
  base::flat_set<std::string> fast_pair_addresses_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::ObserverList<RetroactivePairingDetector::Observer> observers_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::ScopedObservation<PairerBroker, PairerBroker::Observer>
      pairer_broker_observation_{this};
  base::WeakPtrFactory<RetroactivePairingDetectorImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_H_
