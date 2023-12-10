// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_

#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

class Device;
class FastPairFeatureUsageMetricsLogger;

// Observes pairing, scanning and UI events and logs corresponding metrics.
class QuickPairMetricsLogger : public PairerBroker::Observer,
                               public ScannerBroker::Observer,
                               public UIBroker::Observer,
                               public RetroactivePairingDetector::Observer,
                               public device::BluetoothAdapter::Observer {
 public:
  QuickPairMetricsLogger(
      ScannerBroker* scanner_broker,
      PairerBroker* pairer_broker,
      UIBroker* ui_broker,
      RetroactivePairingDetector* retroactive_pairing_detector);
  QuickPairMetricsLogger(const QuickPairMetricsLogger&) = delete;
  QuickPairMetricsLogger& operator=(const QuickPairMetricsLogger&) = delete;
  ~QuickPairMetricsLogger() override;

 private:
  // PairerBroker::Observer
  void OnPairingStart(scoped_refptr<Device> device) override;
  void OnHandshakeComplete(scoped_refptr<Device> device) override;
  void OnDevicePaired(scoped_refptr<Device> device) override;
  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         std::optional<AccountKeyFailure> error) override;
  void OnPairingComplete(scoped_refptr<Device> device) override;
  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override;

  // UIBroker::Observer
  void OnDiscoveryAction(scoped_refptr<Device> device,
                         DiscoveryAction action) override;
  void OnCompanionAppAction(scoped_refptr<Device> device,
                            CompanionAppAction action) override;
  void OnPairingFailureAction(scoped_refptr<Device> device,
                              PairingFailedAction action) override;
  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) override;

  // ScannerBroker::Observer
  void OnDeviceFound(scoped_refptr<Device> device) override;
  void OnDeviceLost(scoped_refptr<Device> device) override;

  // RetroactivePairingDetector::Observer
  void OnRetroactivePairFound(scoped_refptr<Device> device) override;

  // device::BluetoothAdapter::Observer
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Helper function called to get the Bluetooth Device that corresponds with
  // the address saved in |device|. Returns nullptr if the adapter_ is not
  // initialized or the device is not found.
  const device::BluetoothDevice* GetBluetoothDevice(
      scoped_refptr<Device> device) const;

  // Map of devices to the time at which a pairing was initiated. This is used
  // to calculate the time between the user electing to pair the device and
  // the pairing entering a terminal state (success or failure).
  base::flat_map<scoped_refptr<Device>, base::TimeTicks>
      device_pairing_start_timestamps_;

  // Set of devices of which on the Associate Account UI and Discovery UI shown,
  // the Learn More button was pressed. We need this map to know which
  // |FastPairRetroactiveEngagementFlowEvent| or
  // |FastPairEngagementFlowEvent| event to log at the subsequent
  // events that will follow, since the LearnMore event is not a terminal state.
  base::flat_set<scoped_refptr<Device>> associate_account_learn_more_devices_;
  base::flat_set<scoped_refptr<Device>> discovery_learn_more_devices_;

  // The classic pairing addresses of Fast Pair devices that we have already
  // paired to.
  base::flat_set<std::string> fast_pair_addresses_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<FastPairFeatureUsageMetricsLogger>
      feature_usage_metrics_logger_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::ScopedObservation<ScannerBroker, ScannerBroker::Observer>
      scanner_broker_observation_{this};
  base::ScopedObservation<PairerBroker, PairerBroker::Observer>
      pairer_broker_observation_{this};
  base::ScopedObservation<RetroactivePairingDetector,
                          RetroactivePairingDetector::Observer>
      retroactive_pairing_detector_observation_{this};
  base::ScopedObservation<UIBroker, UIBroker::Observer> ui_broker_observation_{
      this};
  base::WeakPtrFactory<QuickPairMetricsLogger> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_
