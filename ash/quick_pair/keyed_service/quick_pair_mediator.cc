// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <cstdint>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
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
#include "ash/quick_pair/repository/fast_pair/device_id_map.h"
#include "ash/quick_pair/repository/fast_pair/device_image_store.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/quick_pair/repository/fast_pair_repository_impl.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker_impl.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace quick_pair {

namespace {

Mediator::Factory* g_test_factory = nullptr;

}

// static
std::unique_ptr<Mediator> Mediator::Factory::Create() {
  if (g_test_factory)
    return g_test_factory->BuildInstance();

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
      std::make_unique<FastPairRepositoryImpl>(), std::move(process_manager));
}

// static
void Mediator::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

Mediator::Mediator(
    std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
    std::unique_ptr<ScannerBroker> scanner_broker,
    std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector,
    std::unique_ptr<MessageStreamLookup> message_stream_lookup,
    std::unique_ptr<PairerBroker> pairer_broker,
    std::unique_ptr<UIBroker> ui_broker,
    std::unique_ptr<FastPairRepository> fast_pair_repository,
    std::unique_ptr<QuickPairProcessManager> process_manager)
    : feature_status_tracker_(std::move(feature_status_tracker)),
      scanner_broker_(std::move(scanner_broker)),
      message_stream_lookup_(std::move(message_stream_lookup)),
      pairer_broker_(std::move(pairer_broker)),
      retroactive_pairing_detector_(std::move(retroactive_pairing_detector)),
      ui_broker_(std::move(ui_broker)),
      fast_pair_repository_(std::move(fast_pair_repository)),
      process_manager_(std::move(process_manager)),
      fast_pair_bluetooth_config_delegate_(
          std::make_unique<FastPairBluetoothConfigDelegate>()) {
  metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
      scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get(),
      retroactive_pairing_detector_.get());
  battery_update_message_handler_ =
      std::make_unique<BatteryUpdateMessageHandler>(
          message_stream_lookup_.get());
  feature_status_tracker_observation_.Observe(feature_status_tracker_.get());
  scanner_broker_observation_.Observe(scanner_broker_.get());
  retroactive_pairing_detector_observation_.Observe(
      retroactive_pairing_detector_.get());
  pairer_broker_observation_.Observe(pairer_broker_.get());
  ui_broker_observation_.Observe(ui_broker_.get());
  config_delegate_observation_.Observe(
      fast_pair_bluetooth_config_delegate_.get());

  // If we already have a discovery session via the Settings pairing dialog,
  // don't start Fast Pair scanning.
  SetFastPairState(feature_status_tracker_->IsFastPairEnabled() &&
                   !has_at_least_one_discovery_session_);
  quick_pair_process::SetProcessManager(process_manager_.get());

  if (ash::features::IsBluetoothRevampEnabled()) {
    // Asynchronously bind to CrosBluetoothConfig so that we don't attempt to
    // bind to it before it has initialized.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Mediator::BindToCrosBluetoothConfig,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
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
  DeviceIdMap::RegisterLocalStatePrefs(registry);
  DeviceImageStore::RegisterLocalStatePrefs(registry);
}

void Mediator::BindToCrosBluetoothConfig() {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveDiscoverySessionStatusChanges(
      cros_discovery_session_observer_receiver_.BindNewPipeAndPassRemote());
}

chromeos::bluetooth_config::FastPairDelegate* Mediator::GetFastPairDelegate() {
  return fast_pair_bluetooth_config_delegate_.get();
}

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  // If we already have a discovery session via the Settings pairing dialog,
  // don't start Fast Pair scanning.
  SetFastPairState(is_enabled && !has_at_least_one_discovery_session_);
}

void Mediator::OnDeviceFound(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  // On discovery, download and decode device images.
  ui_broker_->ShowDiscovery(device);
  fast_pair_repository_->FetchDeviceImages(device);
}

void Mediator::OnDeviceLost(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  ui_broker_->RemoveNotifications();
  FastPairHandshakeLookup::GetInstance()->Erase(device);
}

void Mediator::OnRetroactivePairFound(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  ui_broker_->ShowAssociateAccount(std::move(device));
}

void Mediator::SetFastPairState(bool is_enabled) {
  QP_LOG(VERBOSE) << __func__ << ": " << is_enabled;

  if (is_enabled) {
    scanner_broker_->StartScanning(Protocol::kFastPairInitial);
    return;
  }

  scanner_broker_->StopScanning(Protocol::kFastPairInitial);

  // Clear all existing handshakes.
  FastPairHandshakeLookup::GetInstance()->Clear();

  pairer_broker_->StopPairing();

  // Dismiss all UI notifications.
  ui_broker_->RemoveNotifications();
}

void Mediator::OnDevicePaired(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
  ui_broker_->RemoveNotifications();
  scanner_broker_->OnDevicePaired(device);
  fast_pair_repository_->PersistDeviceImages(device);
}

void Mediator::OnPairFailure(scoped_refptr<Device> device,
                             PairFailure failure) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ",Failure=" << failure;
  ui_broker_->ShowPairingFailed(std::move(device));
}

void Mediator::OnAccountKeyWrite(scoped_refptr<Device> device,
                                 absl::optional<AccountKeyFailure> error) {
  if (!error.has_value()) {
    QP_LOG(INFO) << __func__ << ": Device=" << device;
    return;
  }

  QP_LOG(INFO) << __func__ << ": Device=" << device
               << ",Error=" << error.value();
}

void Mediator::OnDiscoveryAction(scoped_refptr<Device> device,
                                 DiscoveryAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case DiscoveryAction::kPairToDevice: {
      absl::optional<std::vector<uint8_t>> additional_data =
          device->GetAdditionalData(
              Device::AdditionalDataType::kFastPairVersion);

      // Skip showing the in-progress UI for Fast Pair v1 because that pairing
      // is not handled by us E2E.
      if (!additional_data.has_value() || additional_data->size() != 1 ||
          (*additional_data)[0] != 1) {
        ui_broker_->ShowPairing(device);
      }

      pairer_broker_->PairDevice(device);
    } break;
    case DiscoveryAction::kDismissedByUser:
    case DiscoveryAction::kDismissed:
    case DiscoveryAction::kLearnMore:
      break;
  }
}

void Mediator::OnPairingFailureAction(scoped_refptr<Device> device,
                                      PairingFailedAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;
}

void Mediator::OnCompanionAppAction(scoped_refptr<Device> device,
                                    CompanionAppAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;
}

void Mediator::OnAssociateAccountAction(scoped_refptr<Device> device,
                                        AssociateAccountAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case AssociateAccountAction::kAssoicateAccount:
      pairer_broker_->PairDevice(device);
      ui_broker_->RemoveNotifications();
      break;
    case AssociateAccountAction::kLearnMore:
      break;
    case AssociateAccountAction::kDismissedByUser:
    case AssociateAccountAction::kDismissed:
      break;
  }
}

void Mediator::OnAdapterStateControllerChanged(
    chromeos::bluetooth_config::AdapterStateController*
        adapter_state_controller) {
  // Always reset the observation first to handle the case where the ptr
  // became a nullptr (i.e. AdapterStateController was destroyed).
  adapter_state_controller_observation_.Reset();
  if (adapter_state_controller)
    adapter_state_controller_observation_.Observe(adapter_state_controller);
}

void Mediator::OnAdapterStateChanged() {
  chromeos::bluetooth_config::AdapterStateController* adapter_state_controller =
      fast_pair_bluetooth_config_delegate_->adapter_state_controller();
  DCHECK(adapter_state_controller);
  chromeos::bluetooth_config::mojom::BluetoothSystemState adapter_state =
      adapter_state_controller->GetAdapterState();

  // The FeatureStatusTracker already observes when Bluetooth is enabled,
  // disabled, or unavailable. We observe the Bluetooth Config to additionally
  // disable Fast Pair when the adapter is disabling.
  if (adapter_state ==
      chromeos::bluetooth_config::mojom::BluetoothSystemState::kDisabling) {
    QP_LOG(INFO) << __func__ << ": Adapter disabling, disabling Fast Pair.";
    SetFastPairState(false);
  }
}

void Mediator::OnHasAtLeastOneDiscoverySessionChanged(
    bool has_at_least_one_discovery_session) {
  has_at_least_one_discovery_session_ = has_at_least_one_discovery_session;
  QP_LOG(VERBOSE) << __func__
                  << ": Discovery session status changed, we"
                     " have at least one discovery session: "
                  << has_at_least_one_discovery_session_;

  // If we have a discovery session via the Settings pairing dialog, stop
  // Fast Pair scanning. Else, start/stop scanning according to the feature
  // status tracker. Stopping scanning stops all GATT connections that
  // haven't completed their handshake.
  SetFastPairState(!has_at_least_one_discovery_session_ &&
                   feature_status_tracker_->IsFastPairEnabled());
}

}  // namespace quick_pair
}  // namespace ash
