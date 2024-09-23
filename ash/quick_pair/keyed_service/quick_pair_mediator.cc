// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <cstdint>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/companion_app/companion_app_broker_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"
#include "ash/quick_pair/keyed_service/battery_update_message_handler.h"
#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"
#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"
#include "ash/quick_pair/pairing/pairer_broker_impl.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"
#include "ash/quick_pair/repository/fast_pair/device_address_map.h"
#include "ash/quick_pair/repository/fast_pair/device_image_store.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/quick_pair/repository/fast_pair_repository_impl.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker_impl.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace quick_pair {

namespace {

constexpr base::TimeDelta kDismissedDiscoveryNotificationBanTime =
    base::Seconds(2);
constexpr base::TimeDelta kShortBanDiscoveryNotificationBanTime =
    base::Minutes(5);

}  // namespace

std::unique_ptr<Mediator> Mediator::FactoryImpl::BuildInstance() {
  auto process_manager = std::make_unique<QuickPairProcessManagerImpl>();
  auto pairer_broker = std::make_unique<PairerBrokerImpl>();
  auto message_stream_lookup = std::make_unique<MessageStreamLookupImpl>();

  return std::make_unique<Mediator>(
      std::make_unique<FeatureStatusTrackerImpl>(),
      std::make_unique<ScannerBrokerImpl>(process_manager.get()),
      std::make_unique<RetroactivePairingDetectorImpl>(
          pairer_broker.get(), message_stream_lookup.get()),
      std::move(message_stream_lookup), std::move(pairer_broker),
      std::make_unique<UIBrokerImpl>(),
      std::make_unique<CompanionAppBrokerImpl>(),
      std::make_unique<FastPairRepositoryImpl>(), std::move(process_manager));
}

Mediator::Mediator(
    std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
    std::unique_ptr<ScannerBroker> scanner_broker,
    std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector,
    std::unique_ptr<MessageStreamLookup> message_stream_lookup,
    std::unique_ptr<PairerBroker> pairer_broker,
    std::unique_ptr<UIBroker> ui_broker,
    std::unique_ptr<CompanionAppBroker> companion_app_broker,
    std::unique_ptr<FastPairRepository> fast_pair_repository,
    std::unique_ptr<QuickPairProcessManager> process_manager)
    : feature_status_tracker_(std::move(feature_status_tracker)),
      scanner_broker_(std::move(scanner_broker)),
      message_stream_lookup_(std::move(message_stream_lookup)),
      pairer_broker_(std::move(pairer_broker)),
      retroactive_pairing_detector_(std::move(retroactive_pairing_detector)),
      ui_broker_(std::move(ui_broker)),
      companion_app_broker_(std::move(companion_app_broker)),
      fast_pair_repository_(std::move(fast_pair_repository)),
      process_manager_(std::move(process_manager)),
      fast_pair_bluetooth_config_delegate_(
          std::make_unique<FastPairBluetoothConfigDelegate>(
              this /* delegate */)) {
  metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
      scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get(),
      retroactive_pairing_detector_.get());
  battery_update_message_handler_ =
      std::make_unique<BatteryUpdateMessageHandler>(
          message_stream_lookup_.get());
  feature_status_tracker_observation_.Observe(feature_status_tracker_.get());
  companion_app_broker_observation_.Observe(companion_app_broker_.get());
  scanner_broker_observation_.Observe(scanner_broker_.get());
  retroactive_pairing_detector_observation_.Observe(
      retroactive_pairing_detector_.get());
  pairer_broker_observation_.Observe(pairer_broker_.get());
  ui_broker_observation_.Observe(ui_broker_.get());

  // If we already have a discovery session via the Settings pairing dialog,
  // don't start Fast Pair scanning.
  SetFastPairState(feature_status_tracker_->IsFastPairEnabled() &&
                   !has_at_least_one_discovery_session_);
  quick_pair_process::SetProcessManager(process_manager_.get());

  // Asynchronously bind to CrosBluetoothConfig so that we don't attempt to
  // bind to it before it has initialized.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Mediator::BindToCrosBluetoothConfig,
                                weak_ptr_factory_.GetWeakPtr()));
}

Mediator::~Mediator() {
  // The metrics logger must be deleted first because it depends on other
  // members.
  metrics_logger_.reset();
}

// static
void Mediator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  FastPairPrefEnabledProvider::RegisterProfilePrefs(registry);
  SavedDeviceRegistry::RegisterProfilePrefs(registry);
  PendingWriteStore::RegisterProfilePrefs(registry);
}

// static
void Mediator::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DeviceAddressMap::RegisterLocalStatePrefs(registry);
  DeviceImageStore::RegisterLocalStatePrefs(registry);
}

void Mediator::BindToCrosBluetoothConfig() {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveDiscoverySessionStatusChanges(
      cros_discovery_session_observer_receiver_.BindNewPipeAndPassRemote());
}

bluetooth_config::FastPairDelegate* Mediator::GetFastPairDelegate() {
  return fast_pair_bluetooth_config_delegate_.get();
}

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  // If we already have a discovery session via the Settings pairing dialog,
  // don't start Fast Pair scanning.
  SetFastPairState(is_enabled && !has_at_least_one_discovery_session_);

  // Dismiss all in-progress handshakes which will interfere with discovering
  // devices later.
  // TODO(b/229663296): We cancel pairing mid-pair to prevent a crash, but we
  // shouldn't cancel pairing if pairer_broker_->IsPairing() is true.
  if (!is_enabled) {
    CancelPairing();
  }
}

bool Mediator::IsDeviceCurrentlyShowingNotification(
    scoped_refptr<Device> device) {
  // BLE addresses could have rotated, causing this check to return false for
  // the same device. Fast Pair considers a device different if they have
  // different BLE addresses. Similarly, the this check will fail if it is the
  // same physical device under different scenarios: for example, if a device
  // is found via the initial scenario and via the subsequent scenario, Fast
  // Pair does not consider them the same device.
  return device_currently_showing_notification_ &&
         device_currently_showing_notification_->metadata_id() ==
             device->metadata_id() &&
         device_currently_showing_notification_->ble_address() ==
             device->ble_address() &&
         device_currently_showing_notification_->protocol() ==
             device->protocol();
}

bool Mediator::IsDeviceBlockedForDiscoveryNotifications(
    scoped_refptr<Device> device) {
  auto it = discovery_notification_block_list_.find(
      std::make_pair(device->metadata_id(), device->protocol()));
  if (it == discovery_notification_block_list_.end()) {
    return false;
  }

  DiscoveryNotificationDismissalState notification_state = it->second.first;

  // We can reference |ban_expire_time|'s value' directly since we check for
  // `kLongBan` beforehand, and |ban_expire_time| is expected to have a value in
  // all cases except `kLongBan`.
  std::optional<base::Time> ban_expire_time = it->second.second;
  return (notification_state == DiscoveryNotificationDismissalState::kLongBan ||
          base::Time::Now() < ban_expire_time.value());
}

void Mediator::OnDeviceFound(scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << device;

  if (IsDeviceCurrentlyShowingNotification(device)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Extending notification for re-discovered device="
        << device_currently_showing_notification_;
    ui_broker_->ExtendNotification();
    return;
  } else if (device_currently_showing_notification_) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Already showing a notification for a different device="
        << device_currently_showing_notification_;
    return;
  }

  // Because we expect advertisements to be emitted 100ms for discoverable
  // advertisements and 250ms for not discoverable advertisements according to
  // the Fast Pair spec
  // (https://developers.google.com/nearby/fast-pair/specifications/service/provider#advertising_interval_when_discoverable),
  // this means we expect the Mediator’s `OnDeviceFound` event to be triggered
  // frequently for the same device.
  if (IsDeviceBlockedForDiscoveryNotifications(device)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": device is currently blocked for discovery notifications";
    return;
  }

  // Get the device name and add it to the device object, the device will only
  // have a name in the cache if this is a subsequent pairing scenario.
  if (device->protocol() == Protocol::kFastPairSubsequent &&
      device->account_key().has_value()) {
    device->set_display_name(
        fast_pair_repository_->GetDeviceDisplayNameFromCache(
            device->account_key().value()));
  }

  // On discovery, download and decode device images. TODO (b/244472452):
  // remove logic that is executed for every advertisement even if no
  // notification is shown.
  device_currently_showing_notification_ = device;
  ui_broker_->ShowDiscovery(device);
  fast_pair_repository_->FetchDeviceImages(device);

  // Don't modify the delegate's list when flag is disabled.
  if (!features::IsFastPairDevicesBluetoothSettingsEnabled() ||
      device->protocol() != Protocol::kFastPairSubsequent) {
    return;
  }

  // Add device to Subsequent Pairable devices list, AKA Account Linked
  // devices for bluetooth.
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(device);
}

void Mediator::OnDeviceLost(scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << device;

  // Don't modify the delegate's list when flag is disabled.
  if (!features::IsFastPairDevicesBluetoothSettingsEnabled() ||
      device->protocol() != Protocol::kFastPairSubsequent) {
    return;
  }

  // Remove device from Subsequent Pairable devices list, AKA Account Linked
  // devices for bluetooth.
  fast_pair_bluetooth_config_delegate_->RemoveFastPairDevice(device);
}

void Mediator::OnRetroactivePairFound(scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << device;

  // SFUL metrics will cause a crash if Fast Pair is disabled when we
  // retroactive pair, so prevent a notification from popping up.
  // TODO(b/247148054): Look into moving this elsewhere.
  if (!feature_status_tracker_->IsFastPairEnabled()) {
    return;
  }

  // Although at this point in the flow, we have not yet showed a notification
  // a notification will immediately follow after account key writing, so we
  // still want to block Fast Pair pairings until it is complete.
  device_currently_showing_notification_ = device;

  // If a device can retroactively pair, it has a fast pair version higher than
  // V1.
  device->set_version(DeviceFastPairVersion::kHigherThanV1);
  pairer_broker_->PairDevice(device);

  // Try saving mac address to model ID mapping one more time.
  // TODO(b/235117226): we aren't really fetching device images here,
  // since the images are already saved. We just want to save the mapping
  // from mac address to model ID, and for Retroactive Pair this is one
  // of the first times we have mac address and model ID for a paired device.
  fast_pair_repository_->FetchDeviceImages(device);
  fast_pair_repository_->PersistDeviceImages(device);
}

void Mediator::SetFastPairState(bool is_enabled) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << is_enabled;

  if (is_enabled) {
    scanner_broker_->StartScanning(Protocol::kFastPairInitial);
    return;
  }

  scanner_broker_->StopScanning(Protocol::kFastPairInitial);
  ui_broker_->RemoveNotifications();
  discovery_notification_block_list_.clear();
  device_currently_showing_notification_ = nullptr;
}

void Mediator::CancelPairing() {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Clearing handshakes and pairiers.";
  // |pairer_broker_| and its children objects depend on the handshake
  // instance. Shut them down before destroying the handshakes.
  pairer_broker_->StopPairing();
  FastPairHandshakeLookup::GetInstance()->Clear();
  FastPairGattServiceClientLookup::GetInstance()->Clear();

  // Don't modify the delegate's list when flag is disabled.
  if (!features::IsFastPairDevicesBluetoothSettingsEnabled()) {
    return;
  }

  // Clear Subsequent Pairable devices list, AKA Account Linked
  // devices for bluetooth.
  fast_pair_bluetooth_config_delegate_->ClearFastPairableDevices();
}

void Mediator::OnDevicePaired(scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device;
  ui_broker_->RemoveNotifications();
  scanner_broker_->OnDevicePaired(device);

  if (features::IsFastPairPwaCompanionEnabled()) {
    if (!companion_app_broker_->MaybeShowCompanionAppActions(device)) {
      device_currently_showing_notification_ = nullptr;
    }
  } else {
    device_currently_showing_notification_ = nullptr;
  }

  // Try saving mac address to model ID mapping one more time.
  // TODO(b/235117226): we aren't really fetching device images here,
  // since the images are already saved. We just want to save the mapping
  // from mac address to model ID, and for Initial/Subsequent Pair this is one
  // of the first times we have mac address and model ID for a paired device.
  fast_pair_repository_->FetchDeviceImages(device);
  fast_pair_repository_->PersistDeviceImages(device);

  // Unban notifications for this device since it was successfully paired.
  RemoveFromDiscoveryBlockList(device);

  // Don't modify the delegate's list when flag is disabled.
  if (!features::IsFastPairDevicesBluetoothSettingsEnabled() ||
      device->protocol() != Protocol::kFastPairSubsequent) {
    return;
  }

  // Remove device from Subsequent Pairable devices list, AKA Account Linked
  // devices for bluetooth.
  fast_pair_bluetooth_config_delegate_->RemoveFastPairDevice(device);
}

void Mediator::OnPairFailure(scoped_refptr<Device> device,
                             PairFailure failure) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ",Failure=" << failure;
  ui_broker_->ShowPairingFailed(device);

  // Don't modify the delegate's list when flag is disabled.
  if (!features::IsFastPairDevicesBluetoothSettingsEnabled() ||
      device->protocol() != Protocol::kFastPairSubsequent) {
    return;
  }

  // Update device's pairing state to kError.
  fast_pair_bluetooth_config_delegate_->UpdateFastPairableDevicePairingState(
      device, bluetooth_config::mojom::FastPairableDevicePairingState::kError);
}

void Mediator::OnAccountKeyWrite(scoped_refptr<Device> device,
                                 std::optional<AccountKeyFailure> error) {
  if (error.has_value()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Device=" << device << ",Error=" << error.value();
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device;
  if (device->protocol() == Protocol::kFastPairRetroactive) {
    ui_broker_->ShowAssociateAccount(std::move(device));
  }
}

void Mediator::OnDisplayPasskey(std::u16string device_name, uint32_t passkey) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device_name;
  ui_broker_->ShowPasskey(device_name, passkey);
}

void Mediator::UpdateDiscoveryBlockList(scoped_refptr<Device> device) {
  auto it = discovery_notification_block_list_.find(
      std::make_pair(device->metadata_id(), device->protocol()));

  // If this is the first time we are seeing this device, create a new value in
  // the block-list.
  if (it == discovery_notification_block_list_.end()) {
    discovery_notification_block_list_[std::make_pair(device->metadata_id(),
                                                      device->protocol())] =
        std::make_pair(
            DiscoveryNotificationDismissalState::kDismissed,
            std::make_optional(base::Time::Now() +
                               kDismissedDiscoveryNotificationBanTime));
    return;
  }

  // If the device is already in the block-list, update the state and the
  // expire timestamp.
  DiscoveryNotificationDismissalState dismissal_state = it->second.first;
  switch (dismissal_state) {
    case DiscoveryNotificationDismissalState::kDismissed:
      it->second = std::make_pair(
          DiscoveryNotificationDismissalState::kShortBan,
          std::make_optional(base::Time::Now() +
                             kShortBanDiscoveryNotificationBanTime));
      return;
    case DiscoveryNotificationDismissalState::kShortBan:
      // Since `IsDeviceBlockedForDiscoveryNotifications` has an explicit
      // check for `kLongBan`, the timestamp is std::nullopt. The `kLongBan`
      // does not have an expiration timeout.
      it->second = std::make_pair(DiscoveryNotificationDismissalState::kLongBan,
                                  std::nullopt);
      return;
    case DiscoveryNotificationDismissalState::kLongBan:
      // If the device had the state `kLongBan`, it should have never been
      // shown again, so we are expected to never get to this state when a
      // `kLongBan` was shown, and then dismissed by user.
      NOTREACHED();
  }
}

void Mediator::RemoveFromDiscoveryBlockList(scoped_refptr<Device> device) {
  auto key = std::make_pair(device->metadata_id(), device->protocol());
  discovery_notification_block_list_.erase(key);
}

void Mediator::OnDiscoveryAction(scoped_refptr<Device> device,
                                 DiscoveryAction action) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case DiscoveryAction::kPairToDevice: {
      // Skip showing the in-progress UI for Fast Pair v1 because that pairing
      // is not handled by us E2E.
      if (device->version().value() == DeviceFastPairVersion::kHigherThanV1) {
        ui_broker_->ShowPairing(device);
      }

      pairer_broker_->PairDevice(device);

      // Don't modify the delegate's list when flag is disabled.
      if (!features::IsFastPairDevicesBluetoothSettingsEnabled() ||
          device->protocol() != Protocol::kFastPairSubsequent) {
        break;
      }

      // Update device's pairing state to kPairing.
      fast_pair_bluetooth_config_delegate_
          ->UpdateFastPairableDevicePairingState(
              device, bluetooth_config::mojom::FastPairableDevicePairingState::
                          kPairing);
    } break;
    case DiscoveryAction::kDismissedByOs:
      break;
    case DiscoveryAction::kDismissedByUser:
      // When the user explicitly dismisses the discovery notification, update
      // the device's block-list value accordingly.
      UpdateDiscoveryBlockList(device);
      [[fallthrough]];
    case DiscoveryAction::kDismissedByTimeout:
      // When the notification is dismissed by timeout or dismissed by user,
      // there will be no more notifications for |device|. We reset
      // |device_currently_showing_notification_| to enforce the first come,
      // first serve notification strategy to allow other notifications to be
      // shown. We do not do this for `kDismissedByOs` because this is triggered
      // when a discovery notification is removed to be replaced by the
      // connection notification to signify pairing is progress, and thus not
      // in a terminal state, and we do not want to permit other notifications
      // during this time.
      device_currently_showing_notification_ = nullptr;
      FastPairHandshakeLookup::GetInstance()->Erase(device);
      break;
    case DiscoveryAction::kLearnMore:
      break;
  }
}

void Mediator::OnPairingFailureAction(scoped_refptr<Device> device,
                                      PairingFailedAction action) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Action=" << action;
  device_currently_showing_notification_ = nullptr;
}

void Mediator::OnCompanionAppAction(scoped_refptr<Device> device,
                                    CompanionAppAction action) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case CompanionAppAction::kDownloadAndLaunchApp:
      ui_broker_->RemoveNotifications();
      companion_app_broker_->InstallCompanionApp(device);
      device_currently_showing_notification_ = nullptr;
      break;
    case CompanionAppAction::kLaunchApp:
      ui_broker_->RemoveNotifications();
      companion_app_broker_->LaunchCompanionApp(device);
      device_currently_showing_notification_ = nullptr;
      break;
    case CompanionAppAction::kDismissedByUser:
      [[fallthrough]];
    case CompanionAppAction::kDismissed:
      device_currently_showing_notification_ = nullptr;
      break;
  }
}

void Mediator::OnAssociateAccountAction(scoped_refptr<Device> device,
                                        AssociateAccountAction action) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case AssociateAccountAction::kAssociateAccount:
      DCHECK(device->account_key().has_value());
      fast_pair_repository_->WriteAccountAssociationToFootprints(
          device, device->account_key().value());
      ui_broker_->RemoveNotifications();
      device_currently_showing_notification_ = nullptr;
      break;
    case AssociateAccountAction::kDismissedByOs:
      break;
    case AssociateAccountAction::kDismissedByTimeout:
    case AssociateAccountAction::kDismissedByUser:
      // Retroactive pairing only has the associate account notification. If the
      // user elects to save the device or dismisses it, the lifetime of the
      // notification is over and a new one can appear.
      device_currently_showing_notification_ = nullptr;
      break;
    case AssociateAccountAction::kLearnMore:
      break;
  }
}

void Mediator::ShowInstallCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  ui_broker_->ShowInstallCompanionApp(device);
}

void Mediator::ShowLaunchCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  ui_broker_->ShowLaunchCompanionApp(device);
}

// TODO(b/274973687): Implement this function
void Mediator::OnCompanionAppInstalled(scoped_refptr<Device> device) {}

void Mediator::OnAdapterStateControllerChanged(
    bluetooth_config::AdapterStateController* adapter_state_controller) {
  // Always reset the observation first to handle the case where the ptr
  // became a nullptr (i.e. AdapterStateController was destroyed).
  adapter_state_controller_observation_.Reset();
  if (adapter_state_controller) {
    adapter_state_controller_observation_.Observe(adapter_state_controller);
  }
}

void Mediator::OnAdapterStateChanged() {
  bluetooth_config::AdapterStateController* adapter_state_controller =
      fast_pair_bluetooth_config_delegate_->adapter_state_controller();
  DCHECK(adapter_state_controller);
  bluetooth_config::mojom::BluetoothSystemState adapter_state =
      adapter_state_controller->GetAdapterState();

  // The FeatureStatusTracker already observes when Bluetooth is enabled,
  // disabled, or unavailable. We observe the Bluetooth Config to additionally
  // disable Fast Pair when the adapter is disabling.
  if (adapter_state ==
      bluetooth_config::mojom::BluetoothSystemState::kDisabling) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Adapter disabling, disabling Fast Pair.";
    SetFastPairState(false);
    // In addition to stopping scanning, we cancel pairing here to prevent a
    // crash that occurs mid-pair when Bluetooth is disabling.
    CancelPairing();
  }
}

// TODO(b/243586447): Investigate why the classic BT pairing dialog being open
// interferes with Fast Pair GATT connections.
//
// The logic here is necessary to prevent Fast Pair connecting notification
// hanging when Fast Pair pairing has starting and the classic BT pairing
// dialog is open.
void Mediator::OnHasAtLeastOneDiscoverySessionChanged(
    bool has_at_least_one_discovery_session) {
  has_at_least_one_discovery_session_ = has_at_least_one_discovery_session;
  CD_LOG(VERBOSE, Feature::FP) << __func__
                               << ": Discovery session status changed, we"
                                  " have at least one discovery session: "
                               << has_at_least_one_discovery_session_;

  // If we have a discovery session via the Settings pairing dialog, stop
  // Fast Pair scanning. Else, start/stop scanning according to the feature
  // status tracker.
  SetFastPairState(!has_at_least_one_discovery_session_ &&
                   feature_status_tracker_->IsFastPairEnabled());

  // If we haven't begun pairing, dismiss all in-progress handshakes which
  // will interfere with the discovery session. Note that V1 device Fast Pair
  // via the Settings pairing dialog, so we also check for that case here.
  if (has_at_least_one_discovery_session_ && !pairer_broker_->IsPairing()) {
    CancelPairing();
  }
}

}  // namespace quick_pair
}  // namespace ash
