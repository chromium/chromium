// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "base/bind_post_task.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_chromeos.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"

namespace chromeos {
namespace reporting {

bool LoginLogoutReporter::Delegate::ShouldReportUser(
    base::StringPiece user_email) const {
  return ash::ChromeUserManager::Get()->ShouldReportUser(
      std::string(user_email));
}

bool LoginLogoutReporter::Delegate::ShouldReportEvent() const {
  // TODO(crbug.com/1215406): add login/logout reporting policy check.
  return false;
}

policy::DMToken LoginLogoutReporter::Delegate::GetDMToken() const {
  policy::DMToken dm_token(policy::DMToken::Status::kEmpty, "");

  auto* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
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

void LoginLogoutReporter::Delegate::CreateReportingQueue(
    std::unique_ptr<::reporting::ReportQueueConfiguration> config,
    ::reporting::ReportQueueProvider::CreateReportQueueCallback
        create_queue_cb) {
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(::reporting::ReportQueueProvider::CreateQueue,
                                std::move(config), std::move(create_queue_cb)));
}

LoginLogoutReporter::LoginLogoutReporter(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

LoginLogoutReporter::~LoginLogoutReporter() = default;

// static
void LoginLogoutReporter::OnReportQueueCreated(
    LoginLogoutRecord record,
    ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    DVLOG(1) << "ReportQueue could not be created: "
             << report_queue_result.status();
    return;
  }
  auto* const report_queue = report_queue_result.ValueOrDie().release();
  auto enqueue_cb = base::BindOnce(&LoginLogoutReporter::OnRecordEnqueued,
                                   base::Owned(report_queue));
  report_queue->Enqueue(&record, ::reporting::Priority::IMMEDIATE,
                        std::move(enqueue_cb));
}

// static
void LoginLogoutReporter::OnRecordEnqueued(
    ::reporting::ReportQueue* report_queue,
    ::reporting::Status status) {
  if (!status.ok()) {
    DVLOG(1)
        << "Could not enqueue login/logout event to reporting queue because of "
        << status;
  }
}

void LoginLogoutReporter::MaybeReportEvent(base::StringPiece user_email,
                                           LoginLogoutRecord record) const {
  if (delegate_->ShouldReportEvent()) {
    const policy::DMToken dm_token = delegate_->GetDMToken();
    if (!dm_token.is_valid()) {
      DVLOG(1) << "Cannot initialize login/logout reporting. Invalid DMToken.";
      return;
    }

    record.set_event_timestamp(base::Time::Now().ToTimeT());
    if (delegate_->ShouldReportUser(user_email)) {
      record.mutable_affiliated_user()->set_user_email(std::string(user_email));
    }

    auto config_result = ::reporting::ReportQueueConfiguration::Create(
        dm_token.value(), ::reporting::Destination::LOGIN_LOGOUT_EVENTS,
        base::BindRepeating([]() { return ::reporting::Status::StatusOK(); }));

    if (!config_result.ok()) {
      DVLOG(1) << "Cannot initialize login/logout reporting. "
               << "Invalid ReportQueueConfiguration";
      return;
    }

    auto create_queue_cb = base::BindOnce(
        &LoginLogoutReporter::OnReportQueueCreated, std::move(record));
    delegate_->CreateReportingQueue(std::move(config_result.ValueOrDie()),
                                    std::move(create_queue_cb));
  }
}

void LoginLogoutReporter::MaybeReportLogin(base::StringPiece user_email) const {
  LoginLogoutRecord record;
  record.mutable_login_event();
  MaybeReportEvent(user_email, std::move(record));
}

void LoginLogoutReporter::MaybeReportLogout(
    base::StringPiece user_email) const {
  LoginLogoutRecord record;
  record.mutable_logout_event();
  MaybeReportEvent(user_email, std::move(record));
}
}  // namespace reporting
}  // namespace chromeos
