// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"

namespace ash {
namespace eche_app {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

}  // namespace

EcheFeatureStatusProvider::EcheFeatureStatusProvider(
    phonehub::PhoneHubManager* phone_hub_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager,
    EcheConnectionStatusHandler* eche_connection_status_handler)
    : phone_hub_feature_status_provider_(
          phone_hub_manager->GetFeatureStatusProvider()),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager),
      eche_connection_status_handler_(eche_connection_status_handler),
      current_phone_hub_feature_status_(
          phone_hub_feature_status_provider_->GetStatus()),
      status_(ComputeStatus()) {
  phone_hub_feature_status_provider_->AddObserver(this);
  connection_manager_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
}

EcheFeatureStatusProvider::~EcheFeatureStatusProvider() {
  phone_hub_feature_status_provider_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

FeatureStatus EcheFeatureStatusProvider::GetStatus() const {
  return *status_;
}

// Listening for changes in Phone Hub state.
void EcheFeatureStatusProvider::OnFeatureStatusChanged() {
  current_phone_hub_feature_status_ =
      phone_hub_feature_status_provider_->GetStatus();
  UpdateStatus();
}

void EcheFeatureStatusProvider::OnConnectionStatusChanged() {
  UpdateStatus();
}

void EcheFeatureStatusProvider::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  UpdateStatus();
}

void EcheFeatureStatusProvider::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdateStatus();
}

void EcheFeatureStatusProvider::UpdateStatus() {
  DCHECK(status_.has_value());

  FeatureStatus computed_status = ComputeStatus();
  if (computed_status == *status_)
    return;

  PA_LOG(INFO) << "Eche feature status: " << *status_ << " => "
               << computed_status;
  *status_ = computed_status;
  NotifyStatusChanged();

  if (features::IsEcheNetworkConnectionStateEnabled()) {
    // TODO(b/274530047): refactor to make this a normal observer.
    eche_connection_status_handler_->OnFeatureStatusChanged(computed_status);
  }
}

FeatureStatus EcheFeatureStatusProvider::ComputeStatus() {
  switch (current_phone_hub_feature_status_) {
    case phonehub::FeatureStatus::kNotEligibleForFeature:
    case phonehub::FeatureStatus::kEligiblePhoneButNotSetUp:
    case phonehub::FeatureStatus::kPhoneSelectedAndPendingSetup:
    case phonehub::FeatureStatus::kDisabled:
    case phonehub::FeatureStatus::kUnavailableBluetoothOff:
      return FeatureStatus::kDependentFeature;

    case phonehub::FeatureStatus::kEnabledAndConnecting:
    case phonehub::FeatureStatus::kEnabledButDisconnected:
    case phonehub::FeatureStatus::kLockOrSuspended:
      return FeatureStatus::kDependentFeaturePending;

    case phonehub::FeatureStatus::kEnabledAndConnected:
      break;
  }

  switch (multidevice_setup_client_->GetFeatureState(Feature::kEche)) {
    case FeatureState::kDisabledByUser:
    case FeatureState::kUnavailableTopLevelFeatureDisabled:
      return FeatureStatus::kDisabled;

    case FeatureState::kProhibitedByPolicy:
    case FeatureState::kNotSupportedByChromebook:
    case FeatureState::kNotSupportedByPhone:
    case FeatureState::kUnavailableInsufficientSecurity:
    case FeatureState::kUnavailableSuiteDisabled:
    case FeatureState::kUnavailableNoVerifiedHost_ClientNotReady:
    case FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts:
    case FeatureState::
        kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified:
      return FeatureStatus::kIneligible;

    case FeatureState::kEnabledByUser:
      break;
  }

  switch (connection_manager_->GetStatus()) {
    case secure_channel::ConnectionManager::Status::kDisconnected:
      return FeatureStatus::kDisconnected;
    case secure_channel::ConnectionManager::Status::kConnecting:
      return FeatureStatus::kConnecting;
    case secure_channel::ConnectionManager::Status::kConnected:
      return FeatureStatus::kConnected;
  }
}

}  // namespace eche_app
}  // namespace ash
