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
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"

namespace ash {
namespace eche_app {
namespace {

using multidevice::RemoteDeviceRef;
using multidevice::RemoteDeviceRefList;
using multidevice::SoftwareFeature;
using multidevice::SoftwareFeatureState;
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

bool IsEnabledHost(const RemoteDeviceRef& device) {
  return device.GetSoftwareFeatureState(SoftwareFeature::kBetterTogetherHost) !=
             SoftwareFeatureState::kNotSupported &&
         device.GetSoftwareFeatureState(SoftwareFeature::kEcheHost) ==
             SoftwareFeatureState::kEnabled;
}

bool IsEligibleForFeature(
    const std::optional<RemoteDeviceRef>& local_device,
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice host_status,
    const RemoteDeviceRefList& remote_devices,
    FeatureState feature_state) {
  if (feature_state == FeatureState::kProhibitedByPolicy)
    return false;
  if (feature_state == FeatureState::kNotSupportedByPhone)
    return false;
  if (!local_device)
    return false;
  if (local_device->GetSoftwareFeatureState(SoftwareFeature::kEcheClient) ==
      SoftwareFeatureState::kNotSupported)
    return false;
  if (host_status.first == HostStatus::kNoEligibleHosts)
    return false;
  if (host_status.second.has_value()) {
    return IsEnabledHost(*(host_status.second));
  }
  for (const RemoteDeviceRef& device : remote_devices) {
    if (IsEnabledHost(device))
      return true;
  }
  return false;
}

bool IsFeatureDisabledByUser(FeatureState feature_state) {
  return feature_state == FeatureState::kDisabledByUser ||
         feature_state == FeatureState::kUnavailableSuiteDisabled ||
         feature_state == FeatureState::kUnavailableTopLevelFeatureDisabled;
}

}  // namespace

EcheFeatureStatusProvider::EcheFeatureStatusProvider(
    phonehub::PhoneHubManager* phone_hub_manager,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager,
    EcheConnectionStatusHandler* eche_connection_status_handler)
    : phone_hub_feature_status_provider_(
          phone_hub_manager->GetFeatureStatusProvider()),
      device_sync_client_(device_sync_client),
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
  // If PhoneHub is in some degree of unavailability, return that Eche is
  // unavailable.
  switch (current_phone_hub_feature_status_) {
    case phonehub::FeatureStatus::kNotEligibleForFeature:
      [[fallthrough]];
    case phonehub::FeatureStatus::kEligiblePhoneButNotSetUp:
      [[fallthrough]];
    case phonehub::FeatureStatus::kPhoneSelectedAndPendingSetup:
      [[fallthrough]];
    case phonehub::FeatureStatus::kDisabled:
      [[fallthrough]];
    case phonehub::FeatureStatus::kUnavailableBluetoothOff:
      return FeatureStatus::kDependentFeature;
    case phonehub::FeatureStatus::kEnabledAndConnecting:
      [[fallthrough]];
    case phonehub::FeatureStatus::kEnabledButDisconnected:
      [[fallthrough]];
    // The device is in a suspended state.
    case phonehub::FeatureStatus::kLockOrSuspended:
      return FeatureStatus::kDependentFeaturePending;
    case phonehub::FeatureStatus::kEnabledAndConnected:
      break;
  }

  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kEche);

  if (!device_sync_client_->is_ready()) {
    return FeatureStatus::kIneligible;
  }

  if (!IsEligibleForFeature(device_sync_client_->GetLocalDeviceMetadata(),
                            multidevice_setup_client_->GetHostStatus(),
                            device_sync_client_->GetSyncedDevices(),
                            feature_state)) {
    return FeatureStatus::kIneligible;
  }

  if (IsFeatureDisabledByUser(feature_state))
    return FeatureStatus::kDisabled;

  switch (connection_manager_->GetStatus()) {
    case secure_channel::ConnectionManager::Status::kDisconnected:
      return FeatureStatus::kDisconnected;
    case secure_channel::ConnectionManager::Status::kConnecting:
      return FeatureStatus::kConnecting;
    case secure_channel::ConnectionManager::Status::kConnected:
      return FeatureStatus::kConnected;
  }
  PA_LOG(INFO) << "Unexpected feature status state, returning default";
  return FeatureStatus::kDisconnected;
}

}  // namespace eche_app
}  // namespace ash
