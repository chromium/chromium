// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observers/fatal_crash_event_log_observer.h"

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"

namespace policy {

FatalCrashEventLogObserver::FatalCrashEventLogObserver() {
  DeviceCloudPolicyManagerAsh& policy_manager = CHECK_DEREF(
      CHECK_DEREF(
          g_browser_process->platform_part()->browser_policy_connector_ash())
          .GetDeviceCloudPolicyManager());
  reporting::FatalCrashEventsObserver* fatal_crash_events_observer =
      CHECK_DEREF(policy_manager.GetMetricReportingManager())
          .fatal_crash_events_observer();
  if (fatal_crash_events_observer) {
    observation_.Observe(fatal_crash_events_observer);
  }
}

FatalCrashEventLogObserver::~FatalCrashEventLogObserver() = default;

ash::reporting::TriggerEventType FatalCrashEventLogObserver::GetEventType()
    const {
  return ash::reporting::TriggerEventType::FATAL_CRASH;
}

std::set<support_tool::DataCollectorType>
FatalCrashEventLogObserver::GetDataCollectorTypes() const {
  return {support_tool::DataCollectorType::CRASH_IDS,
          support_tool::DataCollectorType::CHROMEOS_CHROME_USER_LOGS,
          support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS};
}

void FatalCrashEventLogObserver::OnFatalCrashEvent(
    const std::string& upload_id) {
  TriggerLogUpload(
      upload_id, base::BindOnce(&FatalCrashEventLogObserver::OnUploadTriggered,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FatalCrashEventLogObserver::OnUploadTriggered(
    EventBasedUploadStatus status) {
  LOG_IF(WARNING, status != EventBasedUploadStatus::kSuccess)
      << "Event based log upload failed for fatal crash event.";
}

}  // namespace policy
