// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <cstdint>
#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"
#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"
#include "ash/quick_pair/pairing/pairer_broker_impl.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/quick_pair/repository/fast_pair_repository_impl.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker_impl.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
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

  return std::make_unique<Mediator>(
      std::make_unique<FeatureStatusTrackerImpl>(),
      std::make_unique<ScannerBrokerImpl>(process_manager.get()),
      std::make_unique<PairerBrokerImpl>(), std::make_unique<UIBrokerImpl>(),
      std::make_unique<FastPairRepositoryImpl>(), std::move(process_manager));
}

// static
void Mediator::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

Mediator::Mediator(std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
                   std::unique_ptr<ScannerBroker> scanner_broker,
                   std::unique_ptr<PairerBroker> pairer_broker,
                   std::unique_ptr<UIBroker> ui_broker,
                   std::unique_ptr<FastPairRepository> fast_pair_repository,
                   std::unique_ptr<QuickPairProcessManager> process_manager)
    : feature_status_tracker_(std::move(feature_status_tracker)),
      scanner_broker_(std::move(scanner_broker)),
      pairer_broker_(std::move(pairer_broker)),
      ui_broker_(std::move(ui_broker)),
      fast_pair_repository_(std::move(fast_pair_repository)),
      process_manager_(std::move(process_manager)) {
  metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
      scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get());

  feature_status_tracker_observation_.Observe(feature_status_tracker_.get());
  scanner_broker_observation_.Observe(scanner_broker_.get());
  pairer_broker_observation_.Observe(pairer_broker_.get());
  ui_broker_observation_.Observe(ui_broker_.get());

  SetFastPairState(feature_status_tracker_->IsFastPairEnabled());
  quick_pair_process::SetProcessManager(process_manager_.get());
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

chromeos::bluetooth_config::FastPairDelegate* Mediator::GetFastPairDelegate() {
  // TODO: Implementation of the delegate and returning the real instance here.
  return nullptr;
}

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  SetFastPairState(is_enabled);
}

void Mediator::OnDeviceFound(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  ui_broker_->ShowDiscovery(device);
}

void Mediator::OnDeviceLost(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  ui_broker_->RemoveNotifications(std::move(device));
}

void Mediator::SetFastPairState(bool is_enabled) {
  QP_LOG(VERBOSE) << __func__ << ": " << is_enabled;

  if (is_enabled)
    scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  else
    scanner_broker_->StopScanning(Protocol::kFastPairInitial);
}

void Mediator::OnDevicePaired(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
  ui_broker_->RemoveNotifications(std::move(device));
}

void Mediator::OnPairFailure(scoped_refptr<Device> device,
                             PairFailure failure) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ",Failure=" << failure;
  ui_broker_->ShowPairingFailed(std::move(device));
}

void Mediator::OnAccountKeyWrite(scoped_refptr<Device> device,
                                 absl::optional<AccountKeyFailure> error) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
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
}

}  // namespace quick_pair
}  // namespace ash
