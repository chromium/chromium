// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/multidevice_feature_access_manager_impl.h"

#include "ash/components/phonehub/connection_scheduler.h"
#include "ash/components/phonehub/message_sender.h"
#include "ash/components/phonehub/pref_names.h"
#include "ash/components/phonehub/util/histogram_util.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "pref_names.h"

namespace ash {
namespace phonehub {

// static
void MultideviceFeatureAccessManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kCameraRollAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
  registry->RegisterIntegerPref(
      prefs::kNotificationAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
  registry->RegisterIntegerPref(
      prefs::kNotificationAccessProhibitedReason,
      static_cast<int>(AccessProhibitedReason::kUnknown));
  registry->RegisterBooleanPref(prefs::kHasDismissedSetupRequiredUi, false);
  registry->RegisterBooleanPref(prefs::kNeedsOneTimeNotificationAccessUpdate,
                                true);
}

MultideviceFeatureAccessManagerImpl::MultideviceFeatureAccessManagerImpl(
    PrefService* pref_service,
    FeatureStatusProvider* feature_status_provider,
    MessageSender* message_sender,
    ConnectionScheduler* connection_scheduler)
    : pref_service_(pref_service),
      feature_status_provider_(feature_status_provider),
      message_sender_(message_sender),
      connection_scheduler_(connection_scheduler) {
  DCHECK(feature_status_provider_);
  DCHECK(message_sender_);

  current_feature_status_ = feature_status_provider_->GetStatus();
  feature_status_provider_->AddObserver(this);
}

MultideviceFeatureAccessManagerImpl::~MultideviceFeatureAccessManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
}

bool MultideviceFeatureAccessManagerImpl::
    HasMultideviceFeatureSetupUiBeenDismissed() const {
  return pref_service_->GetBoolean(prefs::kHasDismissedSetupRequiredUi);
}

void MultideviceFeatureAccessManagerImpl::DismissSetupRequiredUi() {
  pref_service_->SetBoolean(prefs::kHasDismissedSetupRequiredUi, true);
}

MultideviceFeatureAccessManagerImpl::AccessStatus
MultideviceFeatureAccessManagerImpl::GetNotificationAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kNotificationAccessStatus);
  return static_cast<AccessStatus>(status);
}

MultideviceFeatureAccessManagerImpl::AccessStatus
MultideviceFeatureAccessManagerImpl::GetCameraRollAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kCameraRollAccessStatus);
  return static_cast<AccessStatus>(status);
}

MultideviceFeatureAccessManager::AccessStatus
MultideviceFeatureAccessManagerImpl::GetAppsAccessStatus() const {
  // TODO(samchiu): The AppsAccessStatus will be updated by eche_app_ui
  // component only. We should listen to pref change and update it to
  // MultiDeviceFeatureOptInView.
  int status = pref_service_->GetInteger(eche_app::prefs::kAppsAccessStatus);
  return static_cast<AccessStatus>(status);
}

MultideviceFeatureAccessManagerImpl::AccessProhibitedReason
MultideviceFeatureAccessManagerImpl::GetNotificationAccessProhibitedReason()
    const {
  int reason =
      pref_service_->GetInteger(prefs::kNotificationAccessProhibitedReason);
  return static_cast<AccessProhibitedReason>(reason);
}

void MultideviceFeatureAccessManagerImpl::SetNotificationAccessStatusInternal(
    AccessStatus access_status,
    AccessProhibitedReason reason) {
  // TODO(http://crbug.com/1215559): Deprecate when there are no more active
  // Phone Hub notification users on M89. Some users had notifications
  // automatically disabled when updating from M89 to M90+ because the
  // notification feature state went from enabled-by-default to
  // disabled-by-default. To re-enable those users, we once and only once notify
  // observers if access has been granted by the phone. Notably, the
  // MultideviceSetupStateUpdate will decide whether or not the notification
  // feature should be enabled. See MultideviceSetupStateUpdater's method
  // IsWaitingForAccessToInitiallyEnableNotifications() for more details.
  bool needs_one_time_notifications_access_update =
      pref_service_->GetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate) &&
      access_status == AccessStatus::kAccessGranted;

  if (!needs_one_time_notifications_access_update &&
      !HasAccessStatusChanged(access_status, reason)) {
    return;
  }

  pref_service_->SetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate,
                            false);

  PA_LOG(INFO) << "Notification access: "
               << std::make_pair(GetNotificationAccessStatus(),
                                 GetNotificationAccessProhibitedReason())
               << " => " << std::make_pair(access_status, reason);

  pref_service_->SetInteger(prefs::kNotificationAccessStatus,
                            static_cast<int>(access_status));
  pref_service_->SetInteger(prefs::kNotificationAccessProhibitedReason,
                            static_cast<int>(reason));
  NotifyNotificationAccessChanged();

  if (!IsSetupOperationInProgress())
    return;

  switch (access_status) {
    case AccessStatus::kProhibited:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::
              kProhibitedFromProvidingAccess);
      break;
    case AccessStatus::kAccessGranted:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::kCompletedSuccessfully);
      break;
    case AccessStatus::kAvailableButNotGranted:
      // Intentionally blank; the operation status should not change.
      break;
  }
}

void MultideviceFeatureAccessManagerImpl::SetCameraRollAccessStatusInternal(
    AccessStatus camera_roll_access_status) {
  pref_service_->SetInteger(prefs::kCameraRollAccessStatus,
                            static_cast<int>(camera_roll_access_status));
  NotifyCameraRollAccessChanged();
}

void MultideviceFeatureAccessManagerImpl::OnSetupRequested() {
  PA_LOG(INFO) << "Notification access setup flow started.";

  switch (feature_status_provider_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case FeatureStatus::kEnabledAndConnected:
      SendShowNotificationAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case FeatureStatus::kEnabledAndConnecting:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::kConnecting);
      break;
    // We are not connected, so schedule a connection; once the
    // connection succeeds, we'll send the message in OnFeatureStatusChanged().
    case FeatureStatus::kEnabledButDisconnected:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::kConnecting);
      connection_scheduler_->ScheduleConnectionNow();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void MultideviceFeatureAccessManagerImpl::OnFeatureStatusChanged() {
  if (!IsSetupOperationInProgress())
    return;

  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  if (previous_feature_status == current_feature_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnecting &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetNotificationSetupOperationStatus(
        NotificationAccessSetupOperation::Status::kTimedOutConnecting);
    return;
  }

  // If we were previously connected and are now no longer connected, send a
  // connection disconnected state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnected &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetNotificationSetupOperationStatus(
        NotificationAccessSetupOperation::Status::kConnectionDisconnected);
    return;
  }

  if (current_feature_status_ == FeatureStatus::kEnabledAndConnected) {
    SendShowNotificationAccessSetupRequest();
    return;
  }
}

void MultideviceFeatureAccessManagerImpl::
    SendShowNotificationAccessSetupRequest() {
  message_sender_->SendShowNotificationAccessSetupRequest();
  SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status::
          kSentMessageToPhoneAndWaitingForResponse);
}

bool MultideviceFeatureAccessManagerImpl::HasAccessStatusChanged(
    AccessStatus access_status,
    AccessProhibitedReason reason) {
  if (access_status != GetNotificationAccessStatus())
    return true;
  if (access_status == AccessStatus::kProhibited &&
      reason != GetNotificationAccessProhibitedReason()) {
    return true;
  }
  return false;
}

}  // namespace phonehub
}  // namespace ash
