// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
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
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager)
    : eche_connector_(eche_connector),
      message_receiver_(message_receiver),
      feature_status_provider_(feature_status_provider),
      pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager) {
  DCHECK(message_receiver_);
  DCHECK(feature_status_provider_);
  DCHECK(connection_manager_);
  current_feature_status_ = feature_status_provider_->GetStatus();
  current_connection_status_ = connection_manager_->GetStatus();
  feature_status_provider_->AddObserver(this);
  message_receiver_->AddObserver(this);
  connection_manager_->AddObserver(this);
}

AppsAccessManagerImpl::~AppsAccessManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
  message_receiver_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
}

AccessStatus AppsAccessManagerImpl::GetAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kAppsAccessStatus);
  return static_cast<AccessStatus>(status);
}

void AppsAccessManagerImpl::OnSetupRequested() {
  current_feature_status_ = feature_status_provider_->GetStatus();

  if (!IsEligibleForOnboarding(current_feature_status_))
    return;

  switch (connection_manager_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case ConnectionStatus::kConnected:
      SendShowAppsAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case ConnectionStatus::kConnecting:
      SetAppsSetupOperationStatus(
          AppsAccessSetupOperation::Status::kConnecting);
      break;
    // We are not connected, so try to reconnect it. We'll send the message in
    // UpdateSetupOperationState().
    case ConnectionStatus::kDisconnected:
      SetAppsSetupOperationStatus(
          AppsAccessSetupOperation::Status::kConnecting);
      eche_connector_->AttemptNearbyConnection();
      break;
    default:
      break;
  }
}

void AppsAccessManagerImpl::NotifyAppsAccessCanceled() {
  if (connection_manager_->GetStatus() == ConnectionStatus::kDisconnected) {
    base::UmaHistogramEnumeration(
        kEcheOnboardingHistogramName,
        OnboardingUserActionMetric::kFailedConnection);
  } else {
    base::UmaHistogramEnumeration(
        kEcheOnboardingHistogramName,
        OnboardingUserActionMetric::kUserActionCanceled);
  }
}

void AppsAccessManagerImpl::OnGetAppsAccessStateResponseReceived(
    proto::GetAppsAccessStateResponse apps_access_state_response) {
  if (apps_access_state_response.result() == proto::Result::RESULT_NO_ERROR) {
    current_apps_access_state_ = apps_access_state_response.apps_access_state();
    AccessStatus access_status = ComputeAppsAccessState();
    UpdateFeatureEnabledState(GetAccessStatus(), access_status);
    SetAccessStatusInternal(access_status);
  }
}

void AppsAccessManagerImpl::OnSendAppsSetupResponseReceived(
    proto::SendAppsSetupResponse apps_setup_response) {
  if (apps_setup_response.result() == proto::Result::RESULT_NO_ERROR) {
    current_apps_access_state_ = apps_setup_response.apps_access_state();

    // Log the no error response after |current_apps_access_state_| is updated.
    LogAppsSetupResponse(apps_setup_response.result());
    AccessStatus access_status = ComputeAppsAccessState();
    SetAccessStatusInternal(access_status);
  } else if (IsSetupOperationInProgress()) {
    // Log the error response before we change the setup operation to not in
    // progress.
    LogAppsSetupResponse(apps_setup_response.result());
    if (apps_setup_response.result() != proto::Result::RESULT_ACK_BY_EXO) {
      SetAppsSetupOperationStatus(
          (apps_setup_response.result() ==
           proto::Result::RESULT_ERROR_USER_REJECTED)
              ? AppsAccessSetupOperation::Status::kCompletedUserRejected
              : AppsAccessSetupOperation::Status::kOperationFailedOrCancelled);
    }
  }
}

void AppsAccessManagerImpl::OnAppPolicyStateChange(
    proto::AppStreamingPolicy app_policy_state) {
  if (current_app_policy_state_ == app_policy_state)
    return;
  current_app_policy_state_ = app_policy_state;

  // We only notify policy state after we also query access status from the
  // remote phone.
  if (initialized_) {
    AccessStatus access_status = ComputeAppsAccessState();
    UpdateFeatureEnabledState(GetAccessStatus(), access_status);
    SetAccessStatusInternal(access_status);
  }
}

void AppsAccessManagerImpl::OnFeatureStatusChanged() {
  UpdateSetupOperationState();
}

void AppsAccessManagerImpl::OnConnectionStatusChanged() {
  // When this feature is disabled, we will not be able to get
  // OnFeatureStatusChanged() once the connection state changes, we need to
  // listen to OnConnectionStatusChanged() and then trigger the onboarding
  // process. For other cases (eg: feature enabled), OnFeatureStatusChanged has
  // been called, so we return directly.
  if (feature_status_provider_->GetStatus() != FeatureStatus::kDisabled)
    return;

  UpdateSetupOperationState();
}

void AppsAccessManagerImpl::AttemptAppsAccessStateRequest() {
  if (initialized_)
    return;

  const FeatureStatus new_feature_status =
      feature_status_provider_->GetStatus();
  const ConnectionStatus new_connection_status =
      connection_manager_->GetStatus();

  PA_LOG(INFO) << "AttemptAppsAccessStateRequest current_feature_status: "
               << current_feature_status_ << " changed to "
               << new_feature_status
               << " current_connection_status: " << current_connection_status_
               << " changed to " << new_connection_status;

  if (current_feature_status_ == new_feature_status &&
      current_connection_status_ == new_connection_status)
    return;

  if (!IsEligibleForOnboarding(new_feature_status))
    return;

  if (new_connection_status == ConnectionStatus::kDisconnected) {
    eche_connector_->AttemptNearbyConnection();
    return;
  }

  if (new_connection_status == ConnectionStatus::kConnected ||
      new_connection_status == ConnectionStatus::kConnecting) {
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
      [[fallthrough]];
    case AccessStatus::kAvailableButNotGranted:
      // Intentionally blank; the operation status should not change.
      break;
  }
}

AccessStatus AppsAccessManagerImpl::ComputeAppsAccessState() {
  if (current_app_policy_state_ ==
      proto::AppStreamingPolicy::APP_POLICY_DISABLED)
    return AccessStatus::kProhibited;

  if (current_apps_access_state_ == proto::AppsAccessState::ACCESS_GRANTED) {
    return AccessStatus::kAccessGranted;
  }
  return AccessStatus::kAvailableButNotGranted;
}

void AppsAccessManagerImpl::UpdateFeatureEnabledState(
    AccessStatus previous_access_status,
    AccessStatus current_access_status) {
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kEche);
  switch (current_access_status) {
    case AccessStatus::kAccessGranted:
      if (IsPhoneHubEnabled() &&
          previous_access_status == AccessStatus::kAvailableButNotGranted) {
        PA_LOG(INFO) << "Enabling Apps when the access is changed from "
                        "kAvailableButNotGranted to kAccessGranted.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kEche, /*enabled=*/true, /*auth_token=*/std::nullopt,
            base::DoNothing());
      } else if (IsWaitingForAccessToInitiallyEnableApps()) {
        PA_LOG(INFO) << "Enabling Apps for the first time now "
                     << "that access has been granted by the phone.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kEche, /*enabled=*/true, /*auth_token=*/std::nullopt,
            base::DoNothing());
      }
      break;
    case AccessStatus::kProhibited:
      [[fallthrough]];
    case AccessStatus::kAvailableButNotGranted:
      // Disable Apps if apps access has been revoked
      // by the phone.
      if (feature_state == FeatureState::kEnabledByUser) {
        PA_LOG(INFO) << "Disabling kEche feature.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kEche, /*enabled=*/false,
            /*auth_token=*/std::nullopt, base::DoNothing());
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
  return IsPhoneHubEnabled() && multidevice_setup::IsDefaultFeatureEnabledValue(
                                    Feature::kEche, pref_service_);
}

bool AppsAccessManagerImpl::IsPhoneHubEnabled() const {
  return multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub) ==
         FeatureState::kEnabledByUser;
}

void AppsAccessManagerImpl::UpdateSetupOperationState() {
  AttemptAppsAccessStateRequest();

  if (!IsSetupOperationInProgress())
    return;

  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  const ConnectionStatus previous_connection_status =
      current_connection_status_;
  current_connection_status_ = connection_manager_->GetStatus();

  if (previous_feature_status == current_feature_status_ &&
      previous_connection_status == current_connection_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_connection_status == ConnectionStatus::kConnecting &&
      (current_connection_status_ != ConnectionStatus::kConnected ||
       !IsEligibleForOnboarding(current_feature_status_))) {
    SetAppsSetupOperationStatus(
        AppsAccessSetupOperation::Status::kTimedOutConnecting);
    return;
  }

  // If we were previously connected and are now no longer connected, send a
  // connection disconnected state.
  if (previous_connection_status == ConnectionStatus::kConnected &&
      (current_connection_status_ != ConnectionStatus::kConnected ||
       !IsEligibleForOnboarding(current_feature_status_))) {
    SetAppsSetupOperationStatus(
        AppsAccessSetupOperation::Status::kConnectionDisconnected);
    return;
  }

  if (!IsEligibleForOnboarding(current_feature_status_))
    return;

  if (current_connection_status_ == ConnectionStatus::kConnected) {
    SendShowAppsAccessSetupRequest();
  }
}

bool AppsAccessManagerImpl::IsEligibleForOnboarding(
    FeatureStatus feature_status) const {
  return feature_status == FeatureStatus::kConnected ||
         feature_status == FeatureStatus::kConnecting ||
         feature_status == FeatureStatus::kDisconnected ||
         feature_status == FeatureStatus::kDisabled;
}

void AppsAccessManagerImpl::LogAppsSetupResponse(
    proto::Result apps_setup_result) {
  if (!IsSetupOperationInProgress())
    return;

  switch (apps_setup_result) {
    case proto::Result::RESULT_NO_ERROR:
      if (current_apps_access_state_ ==
          proto::AppsAccessState::ACCESS_GRANTED) {
        base::UmaHistogramEnumeration(
            kEcheOnboardingHistogramName,
            OnboardingUserActionMetric::kUserActionPermissionGranted);
      }
      break;
    case proto::Result::RESULT_ERROR_USER_REJECTED:
      base::UmaHistogramEnumeration(
          kEcheOnboardingHistogramName,
          OnboardingUserActionMetric::kUserActionPermissionRejected);
      break;
    case proto::Result::RESULT_ERROR_ACTION_TIMEOUT:
      base::UmaHistogramEnumeration(
          kEcheOnboardingHistogramName,
          OnboardingUserActionMetric::kUserActionTimeout);
      break;
    case proto::Result::RESULT_ERROR_ACTION_CANCELED:
      base::UmaHistogramEnumeration(
          kEcheOnboardingHistogramName,
          OnboardingUserActionMetric::kUserActionRemoteInterrupt);
      break;
    case proto::Result::RESULT_ERROR_SYSTEM:
      base::UmaHistogramEnumeration(kEcheOnboardingHistogramName,
                                    OnboardingUserActionMetric::kSystemError);
      break;
    case proto::Result::RESULT_ACK_BY_EXO:
      base::UmaHistogramEnumeration(kEcheOnboardingHistogramName,
                                    OnboardingUserActionMetric::kAckByExo);
      break;
    default:
      base::UmaHistogramEnumeration(
          kEcheOnboardingHistogramName,
          OnboardingUserActionMetric::kUserActionUnknown);
      break;
  }
}

}  // namespace eche_app
}  // namespace ash
