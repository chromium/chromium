// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_

#include <memory>

#include "ash/quick_pair/companion_app/companion_app_broker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
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

class FastPairRepository;
class Device;
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
      public CompanionAppBroker::Observer,
      public RetroactivePairingDetector::Observer,
      public FastPairBluetoothConfigDelegate::Delegate,
      public bluetooth_config::AdapterStateController::Observer,
      public bluetooth_config::mojom::DiscoverySessionStatusObserver {
 public:
  class Factory {
   public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<Mediator> BuildInstance() = 0;
  };

  class FactoryImpl : public Factory {
   private:
    // Mediator::Factory:
    std::unique_ptr<Mediator> BuildInstance() override;
  };

  Mediator(
      std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
      std::unique_ptr<ScannerBroker> scanner_broker,
      std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector,
      std::unique_ptr<MessageStreamLookup> message_stream_lookup,
      std::unique_ptr<PairerBroker> pairer_broker,
      std::unique_ptr<UIBroker> ui_broker,
      std::unique_ptr<CompanionAppBroker> companion_app_broker,
      std::unique_ptr<FastPairRepository> fast_pair_repository,
      std::unique_ptr<QuickPairProcessManager> process_manager);
  Mediator(const Mediator&) = delete;
  Mediator& operator=(const Mediator&) = delete;
  ~Mediator() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  bluetooth_config::FastPairDelegate* GetFastPairDelegate();

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
                         std::optional<AccountKeyFailure> error) override;
  void OnDisplayPasskey(std::u16string device_name, uint32_t passkey) override;

  // UIBroker::Observer
  void OnDiscoveryAction(scoped_refptr<Device> device,
                         DiscoveryAction action) override;
  void OnPairingFailureAction(scoped_refptr<Device> device,
                              PairingFailedAction action) override;
  void OnCompanionAppAction(scoped_refptr<Device> device,
                            CompanionAppAction action) override;
  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) override;

  // CompanionAppBroker::Observer
  void ShowInstallCompanionApp(scoped_refptr<Device> device) override;
  void ShowLaunchCompanionApp(scoped_refptr<Device> device) override;
  void OnCompanionAppInstalled(scoped_refptr<Device> device) override;

  // RetroactivePairingDetector::Observer
  void OnRetroactivePairFound(scoped_refptr<Device> device) override;

  // FastPairBluetoothConfigDelegate::Delegate
  void OnAdapterStateControllerChanged(bluetooth_config::AdapterStateController*
                                           adapter_state_controller) override;

  // bluetooth_config::AdapterStateController::Observer
  void OnAdapterStateChanged() override;

  // bluetooth_config::mojom::DiscoverySessionStatusObserver
  void OnHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session) override;

 private:
  // Represents a device's most recent state when its found for a discovery
  // notification (initial and subsequent pairing scenarios). This is used
  // to determine if a device can have a notification shown again in the
  // `discovery_notification_block-list_` block-list.
  enum class DiscoveryNotificationDismissalState {
    // If the notification is dismissed, do not show again for 2 seconds.
    kDismissed,
    // If notification is dismissed again, do not show again for 5 minutes.
    kShortBan,
    // If notification is dismissed again, do not show until the block-list is
    // reset, which happens when the user session ends or when the Bluetooth
    // or Fast Pair toggle is toggled off.
    kLongBan,
  };

  void SetFastPairState(bool is_enabled);
  void BindToCrosBluetoothConfig();
  void CancelPairing();

  bool IsDeviceCurrentlyShowingNotification(scoped_refptr<Device> device);
  bool IsDeviceBlockedForDiscoveryNotifications(scoped_refptr<Device> device);
  void UpdateDiscoveryBlockList(scoped_refptr<Device> device);
  void RemoveFromDiscoveryBlockList(scoped_refptr<Device> device);

  bool has_at_least_one_discovery_session_ = false;

  // |device_currently_showing_notification_| can be null if there is no
  // notification currently displayed to user.
  scoped_refptr<Device> device_currently_showing_notification_;

  // The discovery notification block-list, where
  // std::pair<std::string, Protocol> represents the block-list key of the
  // device’s model ID and the pairing protocol corresponding (either initial
  // or subsequent), and the value is
  // std::pair<DiscoveryNotificationDismissalState, std::optional<base::Time>
  // representing the current state of the device and the timestamp of when it
  // is set to expire. It is optional because `kLongBan` does not have an expire
  // timeout. This block-list bans a device model (by model ID), which means
  // that if a user has two of the same device, or two devices are pairing in
  // the same range, both will be blocked for discovery notifications. This is a
  // rare edge case that we consider in order to align with Android by banning
  // by model id. We don’t expect many users to have two of the same device, or
  // two of the same device pairing in the same range at the same time, and
  // users can pair via Bluetooth settings if needed. We cannot use the BLE
  // address as a unique identifier for a device because it rotates, and when
  // Fast Pair shows the discovery notifications, it does not yet have the
  // classic mac address to unique identify a device (this is given as part of
  // the FastPairHandshake).
  base::flat_map<
      std::pair<std::string, Protocol>,
      std::pair<DiscoveryNotificationDismissalState, std::optional<base::Time>>>
      discovery_notification_block_list_;

  std::unique_ptr<FeatureStatusTracker> feature_status_tracker_;
  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<CompanionAppBroker> companion_app_broker_;
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
  base::ScopedObservation<CompanionAppBroker, CompanionAppBroker::Observer>
      companion_app_broker_observation_{this};
  base::ScopedObservation<bluetooth_config::AdapterStateController,
                          bluetooth_config::AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::DiscoverySessionStatusObserver>
      cros_discovery_session_observer_receiver_{this};
  base::WeakPtrFactory<Mediator> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
