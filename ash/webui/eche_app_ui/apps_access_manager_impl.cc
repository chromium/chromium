// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"

#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/services/multidevice_setup/public/cpp/prefs.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace eche_app {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

}  // namespace

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;

// static
void AppsAccessManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kAppsAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
}

AppsAccessManagerImpl::AppsAccessManagerImpl(
    EcheConnector* eche_connector,
    EcheMessageReceiver* message_receiver,
    FeatureStatusProvider* feature_status_provider,
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : eche_connector_(eche_connector),
      message_receiver_(message_receiver),
      feature_status_provider_(feature_status_provider),
      pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client) {
  DCHECK(message_receiver_);
  DCHECK(feature_status_provider_);
  current_feature_status_ = feature_status_provider_->GetStatus();
  feature_status_provider_->AddObserver(this);
  message_receiver_->AddObserver(this);
}

AppsAccessManagerImpl::~AppsAccessManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
  message_receiver_->RemoveObserver(this);
}

AccessStatus AppsAccessManagerImpl::GetAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kAppsAccessStatus);
  return static_cast<AccessStatus>(status);
}

void AppsAccessManagerImpl::OnSetupRequested() {
  switch (feature_status_provider_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case FeatureStatus::kConnected:
      SendShowAppsAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case FeatureStatus::kConnecting:
      SetAppsSetupOperationStatus(
          AppsAccessSetupOperation::Status::kConnecting);
      break;
    // We are not connected, so try to reconnect it. We'll send the message in
    // OnFeatureStatusChanged().
    case FeatureStatus::kDisconnected:
      SetAppsSetupOperationStatus(
          AppsAccessSetupOperation::Status::kConnecting);
      eche_connector_->AttemptNearbyConnection();
      break;
    default:
      break;
  }
}

void AppsAccessManagerImpl::OnGetAppsAccessStateResponseReceived(
    proto::GetAppsAccessStateResponse apps_access_state_response) {
  if (apps_access_state_response.result() == proto::Result::RESULT_NO_ERROR) {
    AccessStatus access_status =
        ComputeAppsAccessState(apps_access_state_response.apps_access_state());
    // TODO(samchiu): Call UpdateFeatureEnabledState to Check access status and
    // disable Eche feature.
    SetAccessStatusInternal(access_status);
  }
}

void AppsAccessManagerImpl::OnSendAppsSetupResponseReceived(
    proto::SendAppsSetupResponse apps_setup_response) {
  if (apps_setup_response.result() == proto::Result::RESULT_NO_ERROR) {
    AccessStatus access_status =
        ComputeAppsAccessState(apps_setup_response.apps_access_state());
    SetAccessStatusInternal(access_status);
  }
}

void AppsAccessManagerImpl::OnFeatureStatusChanged() {
  AttemptAppsAccessStateRequest();

  if (!IsSetupOperationInProgress())
    return;

  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  if (previous_feature_status == current_feature_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_feature_status == FeatureStatus::kConnecting &&
      current_feature_status_ != FeatureStatus::kConnected) {
    SetAppsSetupOperationStatus(
        AppsAccessSetupOperation::Status::kTimedOutConnecting);
    return;
  }

  // If we were previously connected and are now no longer connected, send a
  // connection disconnected state.
  if (previous_feature_status == FeatureStatus::kConnected &&
      current_feature_status_ != FeatureStatus::kConnected) {
    SetAppsSetupOperationStatus(
        AppsAccessSetupOperation::Status::kConnectionDisconnected);
    return;
  }

  if (current_feature_status_ == FeatureStatus::kConnected) {
    SendShowAppsAccessSetupRequest();
  }
}

void AppsAccessManagerImpl::AttemptAppsAccessStateRequest() {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kEchePhoneHubPermissionsOnboarding)) {
    PA_LOG(INFO) << "kEchePhoneHubPermissionsOnboarding flag is false, ignores "
                    "to get apps access status from phone.";
    // TODO: Update to use the actual access value, instead of hard-coding to
    // kAccessGranted.
    pref_service_->SetInteger(prefs::kAppsAccessStatus,
                              static_cast<int>(AccessStatus::kAccessGranted));
    return;
  }

  if (initialized_)
    return;

  const FeatureStatus previous_feature_status = current_feature_status_;
  const FeatureStatus new_feature_status =
      feature_status_provider_->GetStatus();

  if (previous_feature_status == new_feature_status)
    return;

  if (new_feature_status == FeatureStatus::kDisconnected) {
    eche_connector_->AttemptNearbyConnection();
    return;
  }
  if (new_feature_status == FeatureStatus::kConnected ||
      new_feature_status == FeatureStatus::kConnecting) {
    GetAppsAccessStateRequest();
  }
}

void AppsAccessManagerImpl::GetAppsAccessStateRequest() {
  eche_connector_->GetAppsAccessStateRequest();
}

void AppsAccessManagerImpl::SendShowAppsAccessSetupRequest() {
  eche_connector_->SendAppsSetupRequest();
  SetAppsSetupOperationStatus(AppsAccessSetupOperation::Status::
                                  kSentMessageToPhoneAndWaitingForResponse);
}

void AppsAccessManagerImpl::SetAccessStatusInternal(
    AccessStatus access_status) {
  initialized_ = true;
  PA_LOG(INFO) << "Apps access: " << GetAccessStatus() << " => "
               << access_status;

  if (access_status == GetAccessStatus())
    return;

  pref_service_->SetInteger(prefs::kAppsAccessStatus,
                            static_cast<int>(access_status));
  NotifyAppsAccessChanged();

  if (!IsSetupOperationInProgress())
    return;

  switch (access_status) {
    case AccessStatus::kAccessGranted:
      SetAppsSetupOperationStatus(
          AppsAccessSetupOperation::Status::kCompletedSuccessfully);
      break;
    case AccessStatus::kProhibited:
    case AccessStatus::kAvailableButNotGranted:
      // Intentionally blank; the operation status should not change.
      break;
  }
}

AccessStatus AppsAccessManagerImpl::ComputeAppsAccessState(
    proto::AppsAccessState apps_access_state) {
  if (apps_access_state == proto::AppsAccessState::ACCESS_GRANTED) {
    return AccessStatus::kAccessGranted;
  }
  return AccessStatus::kAvailableButNotGranted;
}

void AppsAccessManagerImpl::UpdateFeatureEnabledState(
    AccessStatus access_status) {
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kEche);
  switch (access_status) {
    case AccessStatus::kAccessGranted:
      if (IsWaitingForAccessToInitiallyEnableApps()) {
        PA_LOG(INFO) << "Enabling Apps for the first time now "
                     << "that access has been granted by the phone.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kEche, /*enabled=*/true, /*auth_token=*/absl::nullopt,
            base::DoNothing());
      }
      break;
    case AccessStatus::kProhibited:
    case AccessStatus::kAvailableButNotGranted:
      // Disable Apps if apps access has been revoked
      // by the phone.
      if (feature_state == FeatureState::kEnabledByUser) {
        PA_LOG(INFO) << "Disabling kEche feature.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kEche, /*enabled=*/false,
            /*auth_token=*/absl::nullopt, base::DoNothing());
      }
      break;
  }
}

bool AppsAccessManagerImpl::IsWaitingForAccessToInitiallyEnableApps() const {
  // If the Phone Hub apps feature has never been explicitly set, we should
  // enable it after
  // 1. the top-level Phone Hub feature is enabled, and
  // 2. the phone has granted access.
  // We do *not* want to automatically enable the feature unless the opt-in flow
  // was triggered from this device
  return multidevice_setup::IsDefaultFeatureEnabledValue(Feature::kEche,
                                                         pref_service_) &&
         multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub) ==
             FeatureState::kEnabledByUser;
}

}  // namespace eche_app
}  // namespace ash
