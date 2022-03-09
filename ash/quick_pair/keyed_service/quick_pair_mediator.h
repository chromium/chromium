// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_

#include <memory>

#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;

namespace chromeos {
namespace bluetooth_config {
class FastPairDelegate;
}  // namespace bluetooth_config
}  // namespace chromeos

namespace ash {
namespace quick_pair {

class FastPairBluetoothConfigDelegate;
class FastPairRepository;
struct Device;
class QuickPairProcessManager;
class QuickPairMetricsLogger;
class MessageStreamLookup;
class BatteryUpdateMessageHandler;

// Implements the Mediator design pattern for the components in the Quick Pair
// system, e.g. the UI Broker, Scanning Broker and Pairing Broker.
class Mediator final
    : public FeatureStatusTracker::Observer,
      public ScannerBroker::Observer,
      public PairerBroker::Observer,
      public UIBroker::Observer,
      public RetroactivePairingDetector::Observer,
      public chromeos::bluetooth_config::mojom::DiscoverySessionStatusObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<Mediator> Create();
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   private:
    virtual std::unique_ptr<Mediator> BuildInstance() = 0;
  };

  Mediator(
      std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
      std::unique_ptr<ScannerBroker> scanner_broker,
      std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector,
      std::unique_ptr<MessageStreamLookup> message_stream_lookup,
      std::unique_ptr<PairerBroker> pairer_broker,
      std::unique_ptr<UIBroker> ui_broker,
      std::unique_ptr<FastPairRepository> fast_pair_repository,
      std::unique_ptr<QuickPairProcessManager> process_manager);
  Mediator(const Mediator&) = delete;
  Mediator& operator=(const Mediator&) = delete;
  ~Mediator() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  chromeos::bluetooth_config::FastPairDelegate* GetFastPairDelegate();

  // FeatureStatusTracker::Observer
  void OnFastPairEnabledChanged(bool is_enabled) override;

  // ScannerBroker::Observer
  void OnDeviceFound(scoped_refptr<Device> device) override;
  void OnDeviceLost(scoped_refptr<Device> device) override;

  // PairerBroker::Observer
  void OnDevicePaired(scoped_refptr<Device> device) override;
  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override;
  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         absl::optional<AccountKeyFailure> error) override;

  // UIBroker::Observer
  void OnDiscoveryAction(scoped_refptr<Device> device,
                         DiscoveryAction action) override;
  void OnPairingFailureAction(scoped_refptr<Device> device,
                              PairingFailedAction action) override;
  void OnCompanionAppAction(scoped_refptr<Device> device,
                            CompanionAppAction action) override;
  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) override;

  // RetroactivePairingDetector::Observer
  void OnRetroactivePairFound(scoped_refptr<Device> device) override;

  // chromeos::bluetooth_config::mojom::DiscoverySessionStatusObserver
  void OnHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session) override;

 private:
  void SetFastPairState(bool is_enabled);
  void BindToCrosBluetoothConfig();

  bool has_at_least_one_discovery_session_ = false;
  std::unique_ptr<FeatureStatusTracker> feature_status_tracker_;
  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<FastPairRepository> fast_pair_repository_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  std::unique_ptr<QuickPairMetricsLogger> metrics_logger_;
  std::unique_ptr<FastPairBluetoothConfigDelegate>
      fast_pair_bluetooth_config_delegate_;
  std::unique_ptr<BatteryUpdateMessageHandler> battery_update_message_handler_;

  base::ScopedObservation<FeatureStatusTracker, FeatureStatusTracker::Observer>
      feature_status_tracker_observation_{this};
  base::ScopedObservation<ScannerBroker, ScannerBroker::Observer>
      scanner_broker_observation_{this};
  base::ScopedObservation<PairerBroker, PairerBroker::Observer>
      pairer_broker_observation_{this};
  base::ScopedObservation<RetroactivePairingDetector,
                          RetroactivePairingDetector::Observer>
      retroactive_pairing_detector_observation_{this};
  base::ScopedObservation<UIBroker, UIBroker::Observer> ui_broker_observation_{
      this};
  mojo::Remote<chromeos::bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<
      chromeos::bluetooth_config::mojom::DiscoverySessionStatusObserver>
      cros_discovery_session_observer_receiver_{this};
  base::WeakPtrFactory<Mediator> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
