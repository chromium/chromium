// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/util/status.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

UserEventReporterHelper::UserEventReporterHelper(Destination destination,
                                                 EventType event_type)
    : report_queue_(
          ReportQueueFactory::CreateSpeculativeReportQueue(event_type,
                                                           destination)) {}

UserEventReporterHelper::UserEventReporterHelper(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue)
    : report_queue_(std::move(report_queue)) {}

UserEventReporterHelper::~UserEventReporterHelper() = default;

bool UserEventReporterHelper::ShouldReportUser(const std::string& email) const {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  return ash::ChromeUserManager::Get()->ShouldReportUser(email);
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
}  // namespace reporting
