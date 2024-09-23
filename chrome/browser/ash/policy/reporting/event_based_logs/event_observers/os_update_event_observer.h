// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_OS_UPDATE_EVENT_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_OS_UPDATE_EVENT_OBSERVER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/ash/policy/reporting/os_updates/os_updates_reporter.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"

namespace policy {

// Observes `reporting::OsUpdatesReporter` for OS update failures. When the OS
// update failure event is observed, it uploads the related log files to server.
class OsUpdateEventObserver
    : public EventObserverBase,
      reporting::OsUpdatesReporter::OsUpdateEventBasedLogObserver {
 public:
  OsUpdateEventObserver();
  ~OsUpdateEventObserver() override;

  // EventObserverBase override
  ash::reporting::TriggerEventType GetEventType() const override;
  std::set<support_tool::DataCollectorType> GetDataCollectorTypes()
      const override;

  // reporting::OsUpdatesReporter::OsUpdateEventBasedLogObserver override
  void OnOsUpdateFailed(std::string upload_id) override;

 private:
  void OnUploadTriggered(EventBasedUploadStatus status);

  raw_ref<DeviceCloudPolicyManagerAsh> policy_manager_;
  base::WeakPtrFactory<OsUpdateEventObserver> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_OS_UPDATE_EVENT_OBSERVER_H_
