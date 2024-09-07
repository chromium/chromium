// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace reporting {

namespace {

constexpr char kLoginLogoutReporterDictionary[] =
    "reporting.login_logout_reporter_dictionary";
constexpr char kKioskLoginFailureTimestamp[] = "kiosk_login_failure_timestamp";

PrefService* GetLocalState() {
  if (!g_browser_process || !g_browser_process->local_state()) {
    DVLOG(1) << "Could not get local state.";
    return nullptr;
  }
  return g_browser_process->local_state();
}

LoginLogoutSessionType GetSessionType(const AccountId& account_id) {
  if (account_id == user_manager::GuestAccountId()) {
    return LoginLogoutSessionType::GUEST_SESSION;
  }

  auto type = policy::GetDeviceLocalAccountType(account_id.GetUserEmail());
  if (!type.has_value()) {
    return LoginLogoutSessionType::REGULAR_USER_SESSION;
  }

  switch (type.value()) {
    case policy::DeviceLocalAccountType::kPublicSession:
    case policy::DeviceLocalAccountType::kSamlPublicSession:
      return LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION;
    case policy::DeviceLocalAccountType::kKioskApp:
    case policy::DeviceLocalAccountType::kWebKioskApp:
    case policy::DeviceLocalAccountType::kKioskIsolatedWebApp:
      return LoginLogoutSessionType::KIOSK_SESSION;
  }
  NOTREACHED_IN_MIGRATION();
  return LoginLogoutSessionType::UNSPECIFIED_LOGIN_LOGOUT_SESSION_TYPE;
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
    case AuthFailure::CRYPTOHOME_RECOVERY_SERVICE_ERROR:
    case AuthFailure::CRYPTOHOME_RECOVERY_OAUTH_TOKEN_ERROR:
    case AuthFailure::NUM_FAILURE_REASONS:
      return LoginFailureReason::UNKNOWN_LOGIN_FAILURE_REASON;
  }
}

}  // namespace

AccountId LoginLogoutReporter::Delegate::GetLastLoginAttemptAccountId() const {
  if (!ExistingUserController::current_controller()) {
    return EmptyAccountId();
  }
  return ExistingUserController::current_controller()
      ->GetLastLoginAttemptAccountId();
}

// static
void LoginLogoutReporter::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kLoginLogoutReporterDictionary);
}

LoginLogoutReporter::LoginLogoutReporter(
    std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service,
    base::Clock* clock)
    : reporter_helper_(std::move(reporter_helper)),
      delegate_(std::move(delegate)),
      clock_(clock) {
  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
  MaybeReportKioskLoginFailure();
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
    std::unique_ptr<LoginLogoutReporter::Delegate> delegate,
    policy::ManagedSessionService* managed_session_service,
    base::Clock* clock) {
  return base::WrapUnique(
      new LoginLogoutReporter(std::move(reporter_helper), std::move(delegate),
                              managed_session_service, clock));
}

void LoginLogoutReporter::MaybeReportEvent(LoginLogoutRecord record,
                                           const AccountId& account_id) {
  if (!reporter_helper_->ReportingEnabled(kReportDeviceLoginLogout)) {
    return;
  }

  const LoginLogoutSessionType session_type = GetSessionType(account_id);
  record.set_event_timestamp_sec(clock_->Now().ToTimeT());
  record.set_session_type(session_type);
  const std::string& user_email = account_id.GetUserEmail();
  if (session_type == PUBLIC_ACCOUNT_SESSION || session_type == GUEST_SESSION) {
    record.set_is_guest_session(true);
  } else if (session_type == REGULAR_USER_SESSION) {
    if (reporter_helper_->ShouldReportUser(user_email)) {
      record.mutable_affiliated_user()->set_user_email(user_email);
    } else {
      // This is an unaffiliated user. We can't report any personal information
      // about them, so we report a device-unique user id instead.
      record.mutable_unaffiliated_user()->set_user_id(
          reporter_helper_->GetUniqueUserIdForThisDevice(user_email));
    }
  }
  reporter_helper_->ReportEvent(
      std::make_unique<LoginLogoutRecord>(std::move(record)),
      ::reporting::Priority::SECURITY);
}

void LoginLogoutReporter::OnLogin(Profile* profile) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  LoginLogoutRecord record;
  record.mutable_login_event();
  MaybeReportEvent(std::move(record), user->GetAccountId());
}

void LoginLogoutReporter::OnSessionTerminationStarted(
    const user_manager::User* user) {
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

void LoginLogoutReporter::OnKioskLoginFailure() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!reporter_helper_->ReportingEnabled(kReportDeviceLoginLogout) ||
      !GetLocalState()) {
    return;
  }

  ScopedDictPrefUpdate dict_update(GetLocalState(),
                                   kLoginLogoutReporterDictionary);
  dict_update->Set(kKioskLoginFailureTimestamp,
                   static_cast<int>(clock_->Now().ToTimeT()));
}

void LoginLogoutReporter::MaybeReportKioskLoginFailure() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!GetLocalState()) {
    return;
  }

  const auto* pref =
      GetLocalState()->FindPreference(kLoginLogoutReporterDictionary);
  if (!pref) {
    NOTREACHED_IN_MIGRATION() << "Cannot find pref.";
    return;
  }

  std::optional<int> last_kiosk_login_failure_timestamp =
      pref->GetValue()->GetDict().FindInt(kKioskLoginFailureTimestamp);
  if (!last_kiosk_login_failure_timestamp.has_value()) {
    // No kiosk login failure to report.
    return;
  }

  auto record = std::make_unique<LoginLogoutRecord>();
  record->set_event_timestamp_sec(last_kiosk_login_failure_timestamp.value());
  record->set_session_type(LoginLogoutSessionType::KIOSK_SESSION);
  record->mutable_login_event()->mutable_failure();

  auto enqueue_cb = base::BindOnce([](::reporting::Status status) {
    if (!status.ok()) {
      DVLOG(1) << "Could not enqueue event to reporting queue because of: "
               << status;
      return;
    }

    if (!GetLocalState()) {
      return;
    }
    ScopedDictPrefUpdate dict_update(GetLocalState(),
                                     kLoginLogoutReporterDictionary);
    dict_update->Remove(kKioskLoginFailureTimestamp);
  });

  // Enqueue callback should run on the UI thread (current thread) to access
  // pref service.
  reporter_helper_->ReportEvent(
      std::move(record), ::reporting::Priority::SECURITY,
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(enqueue_cb)));
}

}  // namespace reporting
}  // namespace ash
