// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

using ::content::BrowserThread;

KioskAppLaunchError::Error LoginFailureToKioskAppLaunchError(
    const AuthFailure& error) {
  switch (error.reason()) {
    case AuthFailure::COULD_NOT_MOUNT_TMPFS:
    case AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME:
      return KioskAppLaunchError::Error::kUnableToMount;
    case AuthFailure::DATA_REMOVAL_FAILED:
      return KioskAppLaunchError::Error::kUnableToRemove;
    case AuthFailure::USERNAME_HASH_FAILED:
      return KioskAppLaunchError::Error::kUnableToRetrieveHash;
    default:
      NOTREACHED();
      return KioskAppLaunchError::Error::kUnableToMount;
  }
}

constexpr int kFailedMountRetries = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader::CryptohomedChecker ensures cryptohome daemon is up
// and running by issuing an IsMounted call. If the call does not go through
// and absl::nullopt is not returned, it will retry after some time out and at
// the maximum five times before it gives up. Upon success, it resumes the
// launch by logging in as a kiosk mode account.

class KioskProfileLoader::CryptohomedChecker
    : public base::SupportsWeakPtr<CryptohomedChecker> {
 public:
  explicit CryptohomedChecker(KioskProfileLoader* loader) : loader_(loader) {}
  CryptohomedChecker(const CryptohomedChecker&) = delete;
  CryptohomedChecker& operator=(const CryptohomedChecker&) = delete;
  ~CryptohomedChecker() = default;

  void StartCheck() {
    UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
        &CryptohomedChecker::OnServiceAvailibityChecked, AsWeakPtr()));
  }

 private:
  void Retry() {
    const int kMaxRetryTimes = 5;
    ++retry_count_;
    if (retry_count_ > kMaxRetryTimes) {
      SYSLOG(ERROR) << "Could not talk to cryptohomed for launching kiosk app.";
      ReportCheckResult(KioskAppLaunchError::Error::kCryptohomedNotRunning);
      return;
    }

    const int retry_delay_in_milliseconds = 500 * (1 << retry_count_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&CryptohomedChecker::StartCheck, AsWeakPtr()),
        base::Milliseconds(retry_delay_in_milliseconds));
  }

  void OnServiceAvailibityChecked(bool service_is_ready) {
    if (!service_is_ready) {
      Retry();
      return;
    }

    UserDataAuthClient::Get()->IsMounted(
        user_data_auth::IsMountedRequest(),
        base::BindOnce(&CryptohomedChecker::OnCryptohomeIsMounted,
                       AsWeakPtr()));
  }

  void OnCryptohomeIsMounted(
      absl::optional<user_data_auth::IsMountedReply> reply) {
    if (!reply.has_value()) {
      Retry();
      return;
    }

    // Proceed only when cryptohome is not mounted or running on dev box.
    if (!reply->is_mounted() || !base::SysInfo::IsRunningOnChromeOS()) {
      ReportCheckResult(KioskAppLaunchError::Error::kNone);
    } else {
      SYSLOG(ERROR) << "Cryptohome is mounted before launching kiosk app.";
      ReportCheckResult(KioskAppLaunchError::Error::kAlreadyMounted);
    }
  }

  void ReportCheckResult(KioskAppLaunchError::Error error) {
    if (error == KioskAppLaunchError::Error::kNone) {
      loader_->LoginAsKioskAccount();
    } else {
      loader_->ReportLaunchResult(error);
    }
  }

  KioskProfileLoader* loader_;
  int retry_count_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader

KioskProfileLoader::KioskProfileLoader(const AccountId& app_account_id,
                                       KioskAppType app_type,
                                       Delegate* delegate)
    : account_id_(app_account_id),
      app_type_(app_type),
      delegate_(delegate),
      failed_mount_attempts_(0) {}

KioskProfileLoader::~KioskProfileLoader() = default;

void KioskProfileLoader::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  login_performer_.reset();
  cryptohomed_checker_ = std::make_unique<CryptohomedChecker>(this);
  cryptohomed_checker_->StartCheck();
}

void KioskProfileLoader::LoginAsKioskAccount() {
  login_performer_ =
      std::make_unique<ChromeLoginPerformer>(this, AuthMetricsRecorder::Get());
  switch (app_type_) {
    case KioskAppType::kArcApp:
      login_performer_->LoginAsArcKioskAccount(account_id_);
      return;
    case KioskAppType::kChromeApp:
      login_performer_->LoginAsKioskAccount(account_id_);
      return;
    case KioskAppType::kWebApp:
      login_performer_->LoginAsWebKioskAccount(account_id_);
      return;
  }
}

void KioskProfileLoader::ReportLaunchResult(KioskAppLaunchError::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != KioskAppLaunchError::Error::kNone) {
    delegate_->OnProfileLoadFailed(error);
  }
}

void KioskProfileLoader::OnAuthSuccess(const UserContext& user_context) {
  // LoginPerformer will delete itself.
  login_performer_->set_delegate(nullptr);
  std::ignore = login_performer_.release();

  failed_mount_attempts_ = 0;

  UserSessionManager::GetInstance()->StartSession(
      user_context, UserSessionManager::StartSessionType::kPrimary,
      false,  // has_auth_cookies
      false,  // Start session for user.
      AsWeakPtr());
}

void KioskProfileLoader::OnAuthFailure(const AuthFailure& error) {
  failed_mount_attempts_++;
  if (error.reason() == AuthFailure::UNRECOVERABLE_CRYPTOHOME &&
      failed_mount_attempts_ < kFailedMountRetries) {
    // If the cryptohome mount failed due to corruption of the on disk state -
    // try again: we always ask to "create" cryptohome and the corrupted one
    // was deleted under the hood.
    LoginAsKioskAccount();
    return;
  }
  failed_mount_attempts_ = 0;

  SYSLOG(ERROR) << "Kiosk auth failure: error=" << error.GetErrorString();
  KioskAppLaunchError::SaveCryptohomeFailure(error);
  ReportLaunchResult(LoginFailureToKioskAppLaunchError(error));
}

void KioskProfileLoader::AllowlistCheckFailed(const std::string& email) {
  NOTREACHED();
}

void KioskProfileLoader::PolicyLoadFailed() {
  ReportLaunchResult(KioskAppLaunchError::Error::kPolicyLoadFailed);
}

void KioskProfileLoader::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  delegate_->OnOldEncryptionDetected(std::move(user_context));
}

void KioskProfileLoader::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  delegate_->OnProfileLoaded(profile);
  ReportLaunchResult(KioskAppLaunchError::Error::kNone);
}

}  // namespace ash
