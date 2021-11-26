// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/reporting/client/report_queue_factory.h"

namespace reporting {

UserEventReporterHelper::UserEventReporterHelper(Destination destination)
    : report_queue_(std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()))) {
  policy::DMToken dm_token = GetDMToken();
  if (!dm_token.is_valid()) {
    DVLOG(1) << "Cannot initialize user event reporter. Invalid DMToken.";
    return;
  }
  report_queue_ = ReportQueueFactory::CreateSpeculativeReportQueue(
      dm_token.value(), destination);
}

UserEventReporterHelper::UserEventReporterHelper(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue)
    : report_queue_(std::move(report_queue)) {}

UserEventReporterHelper::~UserEventReporterHelper() = default;

bool UserEventReporterHelper::ShouldReportUser(const std::string& email) const {
  return ash::ChromeUserManager::Get()->ShouldReportUser(email);
}

bool UserEventReporterHelper::ReportingEnabled(
    const std::string& policy_path) const {
  bool enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(policy_path, &enabled);
  return enabled;
}

void UserEventReporterHelper::ReportEvent(
    const google::protobuf::MessageLite* record,
    Priority priority) {
  if (!report_queue_) {
    DVLOG(1) << "Could not enqueue event: null reporting queue";
    return;
  }

  auto enqueue_cb = base::BindOnce([](Status status) {
    if (!status.ok()) {
      DVLOG(1) << "Could not enqueue event to reporting queue because of: "
               << status;
    }
  });
  report_queue_->Enqueue(record, priority, std::move(enqueue_cb));
}

bool UserEventReporterHelper::IsCurrentUserNew() const {
  return user_manager::UserManager::Get()->IsCurrentUserNew();
}

// static
policy::DMToken UserEventReporterHelper::GetDMToken() {
  policy::DMToken dm_token(policy::DMToken::Status::kEmpty, "");

  auto* const connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector) {
    return dm_token;
  }

  auto* const policy_manager = connector->GetDeviceCloudPolicyManager();
  if (policy_manager && policy_manager->IsClientRegistered()) {
    dm_token = policy::DMToken(policy::DMToken::Status::kValid,
                               policy_manager->core()->client()->dm_token());
  }

  return dm_token;
}
}  // namespace reporting
