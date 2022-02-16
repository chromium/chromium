// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"

namespace ash {
namespace reporting {
namespace {

LoginLogoutSessionType GetSessionType(const AccountId& account_id) {
  if (account_id == user_manager::GuestAccountId()) {
    return LoginLogoutSessionType::GUEST_SESSION;
  }

  policy::DeviceLocalAccount::Type type;
  if (!IsDeviceLocalAccountUser(account_id.GetUserEmail(), &type)) {
    return LoginLogoutSessionType::REGULAR_USER_SESSION;
  }

  switch (type) {
    case policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION:
    case policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION:
      return LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION;
    case policy::DeviceLocalAccount::TYPE_KIOSK_APP:
    case policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP:
    case policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP:
      return LoginLogoutSessionType::KIOSK_SESSION;
    default:
      NOTREACHED();
      return LoginLogoutSessionType::UNSPECIFIED_LOGIN_LOGOUT_SESSION_TYPE;
  }
}

LoginFailureReason GetLoginFailureReasonForReport(
    const AuthFailure& error,
    LoginLogoutSessionType session_type) {
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
      return session_type == LoginLogoutSessionType::REGULAR_USER_SESSION
                 ? LoginFailureReason::AUTHENTICATION_ERROR
                 : LoginFailureReason::INTERNAL_LOGIN_FAILURE_REASON;
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
      return LoginFailureReason::UNKNOWN_LOGIN_FAILURE_REASON;
  }
}
}  // namespace

AccountId LoginLogoutReporter::Delegate::GetLastLoginAttemptAccountId() const {
  if (!ash::ExistingUserController::current_controller()) {
    return EmptyAccountId();
  }
  return ash::ExistingUserController::current_controller()
      ->GetLastLoginAttemptAccountId();
}

LoginLogoutReporter::LoginLogoutReporter(
    std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service)
    : reporter_helper_(std::move(reporter_helper)),
      delegate_(std::move(delegate)) {
  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
}

LoginLogoutReporter::~LoginLogoutReporter() = default;

// static
std::unique_ptr<LoginLogoutReporter> LoginLogoutReporter::Create(
    policy::ManagedSessionService* managed_session_service) {
  auto reporter_helper = std::make_unique<::reporting::UserEventReporterHelper>(
      ::reporting::Destination::LOGIN_LOGOUT_EVENTS);
  auto delegate = std::make_unique<LoginLogoutReporter::Delegate>();
  return base::WrapUnique(new LoginLogoutReporter(std::move(reporter_helper),
                                                  std::move(delegate),
                                                  managed_session_service));
}

// static
std::unique_ptr<LoginLogoutReporter> LoginLogoutReporter::CreateForTest(
    std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
    std::unique_ptr<LoginLogoutReporter::Delegate> delegate) {
  return base::WrapUnique(
      new LoginLogoutReporter(std::move(reporter_helper), std::move(delegate),
                              /*managed_session_service=*/nullptr));
}

void LoginLogoutReporter::MaybeReportEvent(LoginLogoutRecord record,
                                           const AccountId& account_id) {
  if (!reporter_helper_->ReportingEnabled(kReportDeviceLoginLogout)) {
    return;
  }

  record.set_event_timestamp_sec(base::Time::Now().ToTimeT());
  const LoginLogoutSessionType session_type = GetSessionType(account_id);
  record.set_session_type(session_type);
  const std::string& user_email = account_id.GetUserEmail();
  if (session_type == LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION) {
    record.set_is_guest_session(true);
  } else if (session_type == LoginLogoutSessionType::REGULAR_USER_SESSION &&
             reporter_helper_->ShouldReportUser(user_email)) {
    record.mutable_affiliated_user()->set_user_email(user_email);
  }

  reporter_helper_->ReportEvent(&record, ::reporting::Priority::SECURITY);
}

void LoginLogoutReporter::OnLogin(Profile* profile) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->IsKioskType()) {
    return;
  }

  LoginLogoutRecord record;
  record.mutable_login_event();
  MaybeReportEvent(std::move(record), user->GetAccountId());
}

void LoginLogoutReporter::OnSessionTerminationStarted(
    const user_manager::User* user) {
  if (user->IsKioskType()) {
    return;
  }

  LoginLogoutRecord record;
  record.mutable_logout_event();
  MaybeReportEvent(std::move(record), user->GetAccountId());
}

void LoginLogoutReporter::OnLoginFailure(const AuthFailure& error) {
  AccountId account_id = delegate_->GetLastLoginAttemptAccountId();
  if (account_id == EmptyAccountId()) {
    return;
  }

  LoginFailureReason failure_reason =
      GetLoginFailureReasonForReport(error, GetSessionType(account_id));
  LoginLogoutRecord record;
  record.mutable_login_event()->mutable_failure()->set_reason(failure_reason);
  MaybeReportEvent(std::move(record), account_id);
}

}  // namespace reporting
}  // namespace ash
