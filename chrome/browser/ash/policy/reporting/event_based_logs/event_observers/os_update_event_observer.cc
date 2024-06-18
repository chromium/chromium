// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observers/os_update_event_observer.h"

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"

namespace policy {

OsUpdateEventObserver::OsUpdateEventObserver()
    : policy_manager_(
          CHECK_DEREF(CHECK_DEREF(g_browser_process->platform_part()
                                      ->browser_policy_connector_ash())
                          .GetDeviceCloudPolicyManager())) {
  policy_manager_->GetOsUpdatesReporter()->AddObserver(this);
}

OsUpdateEventObserver::~OsUpdateEventObserver() {
  policy_manager_->GetOsUpdatesReporter()->RemoveObserver(this);
}

ash::reporting::TriggerEventType OsUpdateEventObserver::GetEventType() const {
  return ash::reporting::TriggerEventType::OS_UPDATE_FAILED;
}

std::set<support_tool::DataCollectorType>
OsUpdateEventObserver::GetDataCollectorTypes() const {
  return {support_tool::DataCollectorType::CHROME_INTERNAL,
          support_tool::DataCollectorType::CHROMEOS_CHROME_USER_LOGS,
          support_tool::DataCollectorType::POLICIES,
          support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS};
}

void OsUpdateEventObserver::OnOsUpdateFailed(std::string upload_id) {
  TriggerLogUpload(upload_id,
                   base::BindOnce(&OsUpdateEventObserver::OnUploadTriggered,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void OsUpdateEventObserver::OnUploadTriggered(EventBasedUploadStatus status) {
  LOG_IF(WARNING, status != EventBasedUploadStatus::kSuccess)
      << "Event based log upload failed for OS update failure event.";
}

}  // namespace policy
