// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_manager.h"

#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;

AppsAccessManager::AppsAccessManager() = default;

AppsAccessManager::~AppsAccessManager() = default;

std::unique_ptr<AppsAccessSetupOperation>
AppsAccessManager::AttemptAppsAccessSetup(
    AppsAccessSetupOperation::Delegate* delegate) {
  base::UmaHistogramEnumeration(
      "Eche.Onboarding.UserAction",
      OnboardingUserActionMetric::kUserActionStartClicked);

  // Should only be able to start the setup process if apps access is
  // available but not yet granted.
  if (GetAccessStatus() != AccessStatus::kAvailableButNotGranted)
    return nullptr;

  int operation_id = next_operation_id_;
  ++next_operation_id_;

  auto operation = base::WrapUnique(new AppsAccessSetupOperation(
      delegate, base::BindOnce(&AppsAccessManager::OnSetupOperationDeleted,
                               weak_ptr_factory_.GetWeakPtr(), operation_id)));
  id_to_operation_map_.emplace(operation_id, operation.get());

  OnSetupRequested();
  return operation;
}

void AppsAccessManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppsAccessManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppsAccessManager::NotifyAppsAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnAppsAccessChanged();
}

void AppsAccessManager::SetAppsSetupOperationStatus(
    AppsAccessSetupOperation::Status new_status) {
  DCHECK(IsSetupOperationInProgress());

  for (auto& it : id_to_operation_map_)
    it.second->NotifyAppsStatusChanged(new_status);

  if (AppsAccessSetupOperation::IsFinalStatus(new_status))
    id_to_operation_map_.clear();
}

bool AppsAccessManager::IsSetupOperationInProgress() const {
  return !id_to_operation_map_.empty();
}

void AppsAccessManager::OnSetupOperationDeleted(int operation_id) {
  auto it = id_to_operation_map_.find(operation_id);
  if (it == id_to_operation_map_.end())
    return;

  id_to_operation_map_.erase(it);

  if (id_to_operation_map_.empty())
    PA_LOG(INFO) << "Apps access setup operation has ended.";
}

}  // namespace eche_app
}  // namespace ash
