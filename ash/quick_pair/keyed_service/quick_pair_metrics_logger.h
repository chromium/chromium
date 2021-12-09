// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_

#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"

namespace ash {
namespace quick_pair {

struct Device;
class FastPairFeatureUsageMetricsLogger;

// Observes pairing, scanning and UI events and logs corresponding metrics.
class QuickPairMetricsLogger : public PairerBroker::Observer,
                               public ScannerBroker::Observer,
                               public UIBroker::Observer {
 public:
  QuickPairMetricsLogger(ScannerBroker* scanner_broker,
                         PairerBroker* pairer_broker,
                         UIBroker* ui_broker);
  QuickPairMetricsLogger(const QuickPairMetricsLogger&) = delete;
  QuickPairMetricsLogger& operator=(const QuickPairMetricsLogger&) = delete;
  ~QuickPairMetricsLogger() override;

 private:
  // PairerBroker::Observer
  void OnDevicePaired(scoped_refptr<Device> device) override;
  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override;
  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         absl::optional<AccountKeyFailure> error) override;

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

  // Map of devices to the time at which a pairing was initiated. This is used
  // to calculate the time between the user electing to pair the device and
  // the pairing entering a terminal state (success or failure).
  base::flat_map<scoped_refptr<Device>, base::TimeTicks>
      device_pairing_start_timestamps_;

  std::unique_ptr<FastPairFeatureUsageMetricsLogger>
      feature_usage_metrics_logger_;
  base::ScopedObservation<ScannerBroker, ScannerBroker::Observer>
      scanner_broker_observation_{this};
  base::ScopedObservation<PairerBroker, PairerBroker::Observer>
      pairer_broker_observation_{this};
  base::ScopedObservation<UIBroker, UIBroker::Observer> ui_broker_observation_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_METRICS_LOGGER_H_
