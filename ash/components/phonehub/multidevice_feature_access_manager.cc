// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/multidevice_feature_access_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace ash {
namespace phonehub {

MultideviceFeatureAccessManager::MultideviceFeatureAccessManager() = default;

MultideviceFeatureAccessManager::~MultideviceFeatureAccessManager() = default;

std::unique_ptr<NotificationAccessSetupOperation>
MultideviceFeatureAccessManager::AttemptNotificationSetup(
    NotificationAccessSetupOperation::Delegate* delegate) {
  // Should only be able to start the setup process if notification access is
  // available but not yet granted.
  // TODO: check camra roll access status once setup flow is wired up
  if (GetNotificationAccessStatus() != AccessStatus::kAvailableButNotGranted)
    return nullptr;

  int operation_id = next_operation_id_;
  ++next_operation_id_;

  auto operation = base::WrapUnique(new NotificationAccessSetupOperation(
      delegate,
      base::BindOnce(&MultideviceFeatureAccessManager::OnSetupOperationDeleted,
                     weak_ptr_factory_.GetWeakPtr(), operation_id)));
  id_to_operation_map_.emplace(operation_id, operation.get());

  OnSetupRequested();
  return operation;
}

void MultideviceFeatureAccessManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MultideviceFeatureAccessManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MultideviceFeatureAccessManager::NotifyNotificationAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnNotificationAccessChanged();
}

void MultideviceFeatureAccessManager::NotifyCameraRollAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnCameraRollAccessChanged();
}

void MultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  DCHECK(IsSetupOperationInProgress());

  PA_LOG(INFO) << "Notification access setup flow - new status: " << new_status;

  for (auto& it : id_to_operation_map_)
    it.second->NotifyStatusChanged(new_status);

  if (NotificationAccessSetupOperation::IsFinalStatus(new_status))
    id_to_operation_map_.clear();
}

bool MultideviceFeatureAccessManager::IsSetupOperationInProgress() const {
  return !id_to_operation_map_.empty();
}

void MultideviceFeatureAccessManager::OnSetupOperationDeleted(
    int operation_id) {
  auto it = id_to_operation_map_.find(operation_id);
  if (it == id_to_operation_map_.end())
    return;

  id_to_operation_map_.erase(it);

  if (id_to_operation_map_.empty())
    PA_LOG(INFO) << "Notification access setup operation has ended.";
}

void MultideviceFeatureAccessManager::Observer::OnNotificationAccessChanged() {
  // Optional method, inherit class doesn't have to implement this
}

void MultideviceFeatureAccessManager::Observer::OnCameraRollAccessChanged() {
  // Optional method, inherit class doesn't have to implement this
}

std::ostream& operator<<(std::ostream& stream,
                         MultideviceFeatureAccessManager::AccessStatus status) {
  switch (status) {
    case MultideviceFeatureAccessManager::AccessStatus::kProhibited:
      stream << "[Access prohibited]";
      break;
    case MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted:
      stream << "[Access available but not granted]";
      break;
    case MultideviceFeatureAccessManager::AccessStatus::kAccessGranted:
      stream << "[Access granted]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    MultideviceFeatureAccessManager::AccessProhibitedReason reason) {
  switch (reason) {
    case MultideviceFeatureAccessManager::AccessProhibitedReason::kUnknown:
      stream << "[Unknown]";
      break;
    case MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile:
      stream << "[Work Profile]";
      break;
    case MultideviceFeatureAccessManager::AccessProhibitedReason::
        kDisabledByPhonePolicy:
      stream << "[Admin Policy]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    std::pair<MultideviceFeatureAccessManager::AccessStatus,
              MultideviceFeatureAccessManager::AccessProhibitedReason>
        status_reason) {
  stream << status_reason.first;
  if (status_reason.first ==
      MultideviceFeatureAccessManager::AccessStatus::kProhibited) {
    stream << "," << status_reason.second;
  }
  return stream;
}

}  // namespace phonehub
}  // namespace ash
