// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

KioskAppLaunchError::Error LoginFailureToKioskAppLaunchError(
    const AuthFailure& error) {
  switch (error.reason()) {
    case AuthFailure::COULD_NOT_MOUNT_TMPFS:
    case AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME:
      return KioskAppLaunchError::UNABLE_TO_MOUNT;
    case AuthFailure::DATA_REMOVAL_FAILED:
      return KioskAppLaunchError::UNABLE_TO_REMOVE;
    case AuthFailure::USERNAME_HASH_FAILED:
      return KioskAppLaunchError::UNABLE_TO_RETRIEVE_HASH;
    default:
      NOTREACHED();
      return KioskAppLaunchError::UNABLE_TO_MOUNT;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader::CryptohomedChecker ensures cryptohome daemon is up
// and running by issuing an IsMounted call. If the call does not go through
// and base::nullopt is not returned, it will retry after some time out and at
// the maximum five times before it gives up. Upon success, it resumes the
// launch by logging in as a kiosk mode account.

class KioskProfileLoader::CryptohomedChecker
    : public base::SupportsWeakPtr<CryptohomedChecker> {
 public:
  explicit CryptohomedChecker(KioskProfileLoader* loader)
      : loader_(loader),
        retry_count_(0) {
  }
  ~CryptohomedChecker() {}

  void StartCheck() {
    CryptohomeClient::Get()->WaitForServiceToBeAvailable(base::Bind(
        &CryptohomedChecker::OnServiceAvailibityChecked, AsWeakPtr()));
  }

 private:
  void Retry() {
    const int kMaxRetryTimes = 5;
    ++retry_count_;
    if (retry_count_ > kMaxRetryTimes) {
      SYSLOG(ERROR) << "Could not talk to cryptohomed for launching kiosk app.";
      ReportCheckResult(KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING);
      return;
    }

    const int retry_delay_in_milliseconds = 500 * (1 << retry_count_);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&CryptohomedChecker::StartCheck, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(retry_delay_in_milliseconds));
  }

  void OnServiceAvailibityChecked(bool service_is_ready) {
    if (!service_is_ready) {
      Retry();
      return;
    }

    CryptohomeClient::Get()->IsMounted(
        base::Bind(&CryptohomedChecker::OnCryptohomeIsMounted, AsWeakPtr()));
  }

  void OnCryptohomeIsMounted(base::Optional<bool> is_mounted) {
    if (!is_mounted.has_value()) {
      Retry();
      return;
    }

    // Proceed only when cryptohome is not mounded or running on dev box.
    if (!is_mounted.value() || !base::SysInfo::IsRunningOnChromeOS()) {
      ReportCheckResult(KioskAppLaunchError::NONE);
    } else {
      SYSLOG(ERROR) << "Cryptohome is mounted before launching kiosk app.";
      ReportCheckResult(KioskAppLaunchError::ALREADY_MOUNTED);
    }
  }

  void ReportCheckResult(KioskAppLaunchError::Error error) {
    if (error == KioskAppLaunchError::NONE)
      loader_->LoginAsKioskAccount();
    else
      loader_->ReportLaunchResult(error);
  }

  KioskProfileLoader* loader_;
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomedChecker);
};


////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader

KioskProfileLoader::KioskProfileLoader(const AccountId& app_account_id,
                                       bool use_guest_mount,
                                       Delegate* delegate)
    : account_id_(app_account_id),
      use_guest_mount_(use_guest_mount),
      delegate_(delegate) {}

KioskProfileLoader::~KioskProfileLoader() {}

void KioskProfileLoader::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  login_performer_.reset();
  cryptohomed_checker_.reset(new CryptohomedChecker(this));
  cryptohomed_checker_->StartCheck();
}

void KioskProfileLoader::LoginAsKioskAccount() {
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginAsKioskAccount(account_id_, use_guest_mount_);
}

void KioskProfileLoader::ReportLaunchResult(KioskAppLaunchError::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != KioskAppLaunchError::NONE) {
    delegate_->OnProfileLoadFailed(error);
  }
}

void KioskProfileLoader::OnAuthSuccess(const UserContext& user_context) {
  // LoginPerformer will delete itself.
  login_performer_->set_delegate(NULL);
  ignore_result(login_performer_.release());

  // If we are launching a demo session, we need to start MountGuest with the
  // guest username; this is because there are several places in the cros code
  // which rely on the username sent to cryptohome to be $guest. Back in Chrome
  // we switch this back to the demo user name to correctly identify this
  // user as a demo user.
  UserContext context = user_context;
  if (context.GetAccountId() == user_manager::GuestAccountId())
    context.SetAccountId(user_manager::DemoAccountId());
  UserSessionManager::GetInstance()->StartSession(
      context, UserSessionManager::PRIMARY_USER_SESSION,
      false,  // has_auth_cookies
      false,  // Start session for user.
      this);
}

void KioskProfileLoader::OnAuthFailure(const AuthFailure& error) {
  SYSLOG(ERROR) << "Kiosk auth failure: error=" << error.GetErrorString();
  KioskAppLaunchError::SaveCryptohomeFailure(error);
  ReportLaunchResult(LoginFailureToKioskAppLaunchError(error));
}

void KioskProfileLoader::WhiteListCheckFailed(const std::string& email) {
  NOTREACHED();
}

void KioskProfileLoader::PolicyLoadFailed() {
  ReportLaunchResult(KioskAppLaunchError::POLICY_LOAD_FAILED);
}

void KioskProfileLoader::SetAuthFlowOffline(bool offline) {
  NOTREACHED();
}

void KioskProfileLoader::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  // This object could be deleted any time after successfully reporting
  // a profile load, so invalidate the delegate now.
  UserSessionManager::GetInstance()->DelegateDeleted(this);

  delegate_->OnProfileLoaded(profile);
  ReportLaunchResult(KioskAppLaunchError::NONE);
}

}  // namespace chromeos
