// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/fake_multidevice_feature_access_manager.h"

namespace ash {
namespace phonehub {

FakeMultideviceFeatureAccessManager::FakeMultideviceFeatureAccessManager(
    AccessStatus notification_access_status,
    AccessStatus camera_roll_access_status,
    AccessStatus apps_access_status,
    AccessProhibitedReason reason)
    : notification_access_status_(notification_access_status),
      camera_roll_access_status_(camera_roll_access_status),
      apps_access_status_(apps_access_status),
      access_prohibited_reason_(reason) {}

FakeMultideviceFeatureAccessManager::~FakeMultideviceFeatureAccessManager() =
    default;

void FakeMultideviceFeatureAccessManager::SetNotificationAccessStatusInternal(
    AccessStatus notification_access_status,
    AccessProhibitedReason reason) {
  if (notification_access_status_ == notification_access_status)
    return;

  notification_access_status_ = notification_access_status;
  access_prohibited_reason_ = reason;
  NotifyNotificationAccessChanged();
}

MultideviceFeatureAccessManager::AccessProhibitedReason
FakeMultideviceFeatureAccessManager::GetNotificationAccessProhibitedReason()
    const {
  return access_prohibited_reason_;
}

void FakeMultideviceFeatureAccessManager::SetCameraRollAccessStatusInternal(
    AccessStatus camera_roll_access_status) {
  if (camera_roll_access_status_ == camera_roll_access_status)
    return;

  camera_roll_access_status_ = camera_roll_access_status;
  NotifyCameraRollAccessChanged();
}

void FakeMultideviceFeatureAccessManager::SetAppsAccessStatusInternal(
    AccessStatus apps_access_status) {
  if (apps_access_status_ == apps_access_status)
    return;

  apps_access_status_ = apps_access_status;
}

MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetAppsAccessStatus() const {
  return apps_access_status_;
}

MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetNotificationAccessStatus() const {
  return notification_access_status_;
}
MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetCameraRollAccessStatus() const {
  return camera_roll_access_status_;
}

bool FakeMultideviceFeatureAccessManager::
    HasMultideviceFeatureSetupUiBeenDismissed() const {
  return has_notification_setup_ui_been_dismissed_;
}

void FakeMultideviceFeatureAccessManager::DismissSetupRequiredUi() {
  has_notification_setup_ui_been_dismissed_ = true;
}

void FakeMultideviceFeatureAccessManager::
    ResetHasMultideviceFeatureSetupUiBeenDismissed() {
  has_notification_setup_ui_been_dismissed_ = false;
}

void FakeMultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  switch (new_status) {
    case NotificationAccessSetupOperation::Status::kCompletedSuccessfully:
      SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                          AccessProhibitedReason::kUnknown);
      break;
    case NotificationAccessSetupOperation::Status::
        kProhibitedFromProvidingAccess:
      SetNotificationAccessStatusInternal(
          AccessStatus::kProhibited,
          AccessProhibitedReason::kDisabledByPhonePolicy);
      break;
    default:
      // Do not update access status based on other operation status values.
      break;
  }

  MultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
      new_status);
}

}  // namespace phonehub
}  // namespace ash
