// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/scoped_observation.h"

namespace ash {
namespace quick_pair {

// Implements the Mediator design pattern for the components in the Quick Pair
// system, e.g. the UI Broker, Scanning Broker and Pairing Broker.
class Mediator : public FeatureStatusTracker::Observer,
                 public ScannerBroker::Observer,
                 public UIBroker::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<Mediator> Create();
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   private:
    virtual std::unique_ptr<Mediator> BuildInstance() = 0;
  };

  Mediator(std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
           std::unique_ptr<ScannerBroker> scanner_broker,
           std::unique_ptr<UIBroker> ui_broker);
  Mediator(const Mediator&) = delete;
  Mediator& operator=(const Mediator&) = delete;
  ~Mediator() final;

  // QuickPairFeatureStatusTracker::Observer
  void OnFastPairEnabledChanged(bool is_enabled) override;

  // SannerBroker::Observer
  void OnDeviceFound(const Device& device) override;
  void OnDeviceLost(const Device& device) override;

  // UIBroker::Observer
  void OnDiscoveryAction(const Device& device, DiscoveryAction action) override;
  void OnPairingFailureAction(const Device& device,
                              PairingFailedAction action) override;
  void OnCompanionAppAction(const Device& device,
                            CompanionAppAction action) override;
  void OnAssociateAccountAction(const Device& device,
                                AssociateAccountAction action) override;

 private:
  void SetFastPairState(bool is_enabled);

  std::unique_ptr<FeatureStatusTracker> feature_status_tracker_;
  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<UIBroker> ui_broker_;

  base::ScopedObservation<FeatureStatusTracker, FeatureStatusTracker::Observer>
      feature_status_tracker_observation_{this};
  base::ScopedObservation<ScannerBroker, ScannerBroker::Observer>
      scanner_broker_observation_{this};
  base::ScopedObservation<UIBroker, UIBroker::Observer> ui_broker_observation_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
