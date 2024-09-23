// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include <utility>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

UserEventReporterHelper::UserEventReporterHelper(Destination destination,
                                                 EventType event_type)
    : report_queue_(ReportQueueFactory::CreateSpeculativeReportQueue(
          [](Destination destination, EventType event_type) {
            SourceInfo source_info;
            source_info.set_source(SourceInfo::ASH);
            return ReportQueueConfiguration::Create(
                       {.event_type = event_type, .destination = destination})
                .SetSourceInfo(std::move(source_info));
          }(destination, event_type))) {}

UserEventReporterHelper::UserEventReporterHelper(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue)
    : report_queue_(std::move(report_queue)) {}

UserEventReporterHelper::~UserEventReporterHelper() = default;

bool UserEventReporterHelper::ShouldReportUser(const std::string& email) const {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  auto* reporting_user_tracker = g_browser_process->platform_part()
                                     ->browser_policy_connector_ash()
                                     ->GetDeviceCloudPolicyManager()
                                     ->reporting_user_tracker();
  return reporting_user_tracker->ShouldReportUser(email);
}

bool UserEventReporterHelper::ReportingEnabled(
    const std::string& policy_path) const {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  bool enabled = false;
  ash::CrosSettings::Get()->GetBoolean(policy_path, &enabled);
  return enabled;
}

bool UserEventReporterHelper::IsKioskUser() const {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  auto* const primary = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary) {
    return false;
  }
  return primary->IsKioskType();
}

void UserEventReporterHelper::ReportEvent(
    std::unique_ptr<const google::protobuf::MessageLite> record,
    Priority priority,
    ReportQueue::EnqueueCallback enqueue_cb) {
  if (!report_queue_) {
    std::move(enqueue_cb)
        .Run(Status(error::UNAVAILABLE, "Reporting queue is null."));
    base::UmaHistogramEnumeration(reporting::kUmaUnavailableErrorReason,
                                  UnavailableErrorReason::REPORT_QUEUE_IS_NULL,
                                  UnavailableErrorReason::MAX_VALUE);
    return;
  }
  report_queue_->Enqueue(std::move(record), priority, std::move(enqueue_cb));
}

bool UserEventReporterHelper::IsCurrentUserNew() const {
  return user_manager::UserManager::Get()->IsCurrentUserNew();
}

// static
scoped_refptr<base::SequencedTaskRunner>
UserEventReporterHelper::valid_task_runner() {
  return ::content::GetUIThreadTaskRunner({});
}

// static
void UserEventReporterHelper::OnEnqueueDefault(Status status) {
  if (!status.ok()) {
    DVLOG(1) << "Could not enqueue event to reporting queue because of: "
             << status;
  }
}

std::string UserEventReporterHelper::GetDeviceDmToken() const {
  const enterprise_management::PolicyData* const policy_data =
      ash::DeviceSettingsService::Get()->policy_data();
  if (policy_data && policy_data->has_request_token()) {
    return policy_data->request_token();
  }
  return std::string();
}

std::string UserEventReporterHelper::GetUniqueUserIdForThisDevice(
    std::string_view user_email) const {
  const std::string device_dm_token = GetDeviceDmToken();
  return device_dm_token.empty()
             ? device_dm_token
             : base::NumberToString(base::PersistentHash(
                   base::StrCat({user_email, device_dm_token})));
}
}  // namespace reporting
