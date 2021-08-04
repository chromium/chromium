// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "base/bind_post_task.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"

namespace chromeos {
namespace reporting {

LoginLogoutReporter::Delegate::Delegate() {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  sequenced_task_runner_ = base::SequencedTaskRunnerHandle::Get();
}

LoginLogoutReporter::Delegate::~Delegate() = default;

bool LoginLogoutReporter::Delegate::ShouldReportUser(
    base::StringPiece user_email) const {
  if (!ash::ChromeUserManager::Get())
    return false;

  return ash::ChromeUserManager::Get()->ShouldReportUser(
      std::string(user_email));
}

bool LoginLogoutReporter::Delegate::ShouldReportEvent() const {
  bool report_login_logout = false;
  if (chromeos::CrosSettings::Get()) {
    chromeos::CrosSettings::Get()->GetBoolean(
        chromeos::kReportDeviceLoginLogout, &report_login_logout);
  }
  return report_login_logout;
}

policy::DMToken LoginLogoutReporter::Delegate::GetDMToken() const {
  policy::DMToken dm_token(policy::DMToken::Status::kEmpty, "");

  auto* const connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();

  if (!connector)
    return dm_token;

  auto* const policy_manager = connector->GetDeviceCloudPolicyManager();
  if (policy_manager && policy_manager->IsClientRegistered()) {
    dm_token = policy::DMToken(policy::DMToken::Status::kValid,
                               policy_manager->core()->client()->dm_token());
  }

  return dm_token;
}

AccountId LoginLogoutReporter::Delegate::GetLastLoginAttemptAccountId() const {
  if (!ash::ExistingUserController::current_controller()) {
    return EmptyAccountId();
  }
  return ash::ExistingUserController::current_controller()
      ->GetLastLoginAttemptAccountId();
}

void LoginLogoutReporter::Delegate::CreateReportingQueue(
    std::unique_ptr<::reporting::ReportQueueConfiguration> config,
    ::reporting::ReportQueueProvider::CreateReportQueueCallback
        create_queue_cb) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(::reporting::ReportQueueProvider::CreateQueue,
                     std::move(config),
                     base::BindPostTask(sequenced_task_runner_,
                                        std::move(create_queue_cb))));
}

LoginLogoutReporter::LoginLogoutReporter(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  Init();
  managed_session_observation_.Observe(&managed_session_service_);
}

LoginLogoutReporter::~LoginLogoutReporter() = default;

void LoginLogoutReporter::Init() {
  is_initialized_ = true;
  const policy::DMToken dm_token = delegate_->GetDMToken();
  if (!dm_token.is_valid()) {
    DVLOG(1) << "Cannot initialize login/logout reporting. Invalid DMToken.";
    is_initialized_ = false;
    return;
  }

  auto config_result = ::reporting::ReportQueueConfiguration::Create(
      dm_token.value(), ::reporting::Destination::LOGIN_LOGOUT_EVENTS,
      base::BindRepeating([]() { return ::reporting::Status::StatusOK(); }));

  if (!config_result.ok()) {
    DVLOG(1) << "Cannot initialize login/logout reporting. "
             << "Invalid ReportQueueConfiguration";
    is_initialized_ = false;
    return;
  }

  auto create_queue_cb =
      base::BindOnce(&LoginLogoutReporter::OnReportQueueCreated,
                     weak_ptr_factory_.GetWeakPtr());
  delegate_->CreateReportingQueue(std::move(config_result.ValueOrDie()),
                                  std::move(create_queue_cb));
}

void LoginLogoutReporter::OnReportQueueCreated(
    ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    DVLOG(1) << "ReportQueue could not be created: "
             << report_queue_result.status();
    return;
  }
  if (!report_queue_) {
    report_queue_ = std::move(report_queue_result.ValueOrDie());
  }
  while (!pending_events_.empty()) {
    EnqueueRecord(pending_events_.front());
    pending_events_.pop();
  }
}

void LoginLogoutReporter::EnqueueRecord(const LoginLogoutRecord& record) {
  auto enqueue_cb = base::BindOnce(&LoginLogoutReporter::OnRecordEnqueued);
  report_queue_->Enqueue(&record, ::reporting::Priority::IMMEDIATE,
                         std::move(enqueue_cb));
}

// static
void LoginLogoutReporter::OnRecordEnqueued(::reporting::Status status) {
  if (!status.ok()) {
    DVLOG(1)
        << "Could not enqueue login/logout event to reporting queue because of "
        << status;
  }
}

void LoginLogoutReporter::MaybeReportEvent(LoginLogoutRecord record,
                                           base::StringPiece user_email,
                                           bool is_guest) {
  if (!delegate_->ShouldReportEvent())
    return;

  record.set_event_timestamp(base::Time::Now().ToTimeT());
  if (is_guest) {
    record.set_is_guest_session(true);
  } else if (delegate_->ShouldReportUser(user_email)) {
    record.mutable_affiliated_user()->set_user_email(std::string(user_email));
  }

  if (report_queue_) {
    EnqueueRecord(record);
    return;
  }

  pending_events_.push(std::move(record));
  if (!is_initialized_) {
    Init();
  }
}

void LoginLogoutReporter::OnLogin(Profile* profile) {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->IsKioskType())
    return;

  LoginLogoutRecord record;
  record.mutable_login_event();
  MaybeReportEvent(std::move(record), user->GetAccountId().GetUserEmail(),
                   user->GetType() == user_manager::USER_TYPE_GUEST);
}

void LoginLogoutReporter::OnSessionTerminationStarted(
    const user_manager::User* user) {
  if (user->IsKioskType())
    return;

  LoginLogoutRecord record;
  record.mutable_logout_event();
  MaybeReportEvent(std::move(record), user->GetAccountId().GetUserEmail(),
                   user->GetType() == user_manager::USER_TYPE_GUEST);
}

// static
LoginFailureReason LoginLogoutReporter::GetLoginFailureReasonForReport(
    const chromeos::AuthFailure& error) {
  switch (error.reason()) {
    case AuthFailure::OWNER_REQUIRED:
      return LoginFailureReason::OWNER_REQUIRED;
    case AuthFailure::TPM_ERROR:
      return LoginFailureReason::TPM_ERROR;
    case AuthFailure::TPM_UPDATE_REQUIRED:
      return LoginFailureReason::TPM_UPDATE_REQUIRED;
    case AuthFailure::MISSING_CRYPTOHOME:
      return LoginFailureReason::MISSING_CRYPTOHOME;
    case AuthFailure::UNRECOVERABLE_CRYPTOHOME:
      return LoginFailureReason::UNRECOVERABLE_CRYPTOHOME;
    case AuthFailure::COULD_NOT_MOUNT_TMPFS:
      return LoginFailureReason::COULD_NOT_MOUNT_TMPFS;
    case AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME:
    case AuthFailure::DATA_REMOVAL_FAILED:
    case AuthFailure::USERNAME_HASH_FAILED:
    case AuthFailure::FAILED_TO_INITIALIZE_TOKEN:
      return LoginFailureReason::AUTHENTICATION_ERROR;
    // The following cases are not expected with failed logins, but we add them
    // to fail compliation in case a new relevant auth failure reason was added
    // and we need to add the corresponding enum value to the reporting proto.
    case AuthFailure::NONE:
    case AuthFailure::COULD_NOT_UNMOUNT_CRYPTOHOME:
    case AuthFailure::LOGIN_TIMED_OUT:
    case AuthFailure::UNLOCK_FAILED:
    case AuthFailure::NETWORK_AUTH_FAILED:
    case AuthFailure::ALLOWLIST_CHECK_FAILED:
    case AuthFailure::AUTH_DISABLED:
    case AuthFailure::NUM_FAILURE_REASONS:
      return LoginFailureReason::UNKNOWN;
  }
}

void LoginLogoutReporter::OnLoginFailure(const chromeos::AuthFailure& error) {
  AccountId account_id = delegate_->GetLastLoginAttemptAccountId();
  if (account_id == EmptyAccountId())
    return;

  bool is_guest = account_id == user_manager::GuestAccountId();
  LoginFailureReason failure_reason = GetLoginFailureReasonForReport(error);
  LoginLogoutRecord record;
  record.mutable_login_event()->mutable_failure()->set_reason(failure_reason);
  MaybeReportEvent(std::move(record), account_id.GetUserEmail(), is_guest);
}
}  // namespace reporting
}  // namespace chromeos
