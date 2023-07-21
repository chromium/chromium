// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"

#include <memory>
#include <tuple>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/retry_runner.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

enum class MountedState { kMounted, kNotMounted };

using CryptohomeMountStateCallback =
    base::OnceCallback<void(absl::optional<MountedState> result)>;

bool IsTestOrLinuxChromeOS() {
  // This code should only run in Chrome OS, so not `IsRunningOnChromeOS()`
  // means it's either a test or linux-chromeos.
  return !base::SysInfo::IsRunningOnChromeOS();
}

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

absl::optional<MountedState> ToResult(
    absl::optional<user_data_auth::IsMountedReply> reply) {
  if (!reply.has_value()) {
    return absl::nullopt;
  }
  if (IsTestOrLinuxChromeOS()) {
    // In tests and in linux-chromeos there is no real cryptohome, and the fake
    // one always replies with `is_mounted()` true. We override the reply so
    // Kiosk login can proceed.
    return MountedState::kNotMounted;
  }
  return reply->is_mounted() ? MountedState::kMounted
                             : MountedState::kNotMounted;
}

void CheckCryptohomeMountState(CryptohomeMountStateCallback on_done) {
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      [](CryptohomeMountStateCallback on_done, bool service_is_ready) {
        if (!service_is_ready || !UserDataAuthClient::Get()) {
          return std::move(on_done).Run(absl::nullopt);
        }

        UserDataAuthClient::Get()->IsMounted(
            user_data_auth::IsMountedRequest(),
            base::BindOnce(&ToResult).Then(std::move(on_done)));
      },
      std::move(on_done)));
}

// Checks the mount state of cryptohome and retries if the cryptohome service is
// not yet available.
std::unique_ptr<CancellableJob> CheckCryptohome(
    CryptohomeMountStateCallback on_done) {
  return RunUpToNTimes<MountedState>(
      /*n=*/5,
      /*job=*/base::BindRepeating(&CheckCryptohomeMountState),
      /*on_done=*/std::move(on_done));
}

class SessionStarter : public CancellableJob,
                       public UserSessionManagerDelegate {
 public:
  using ResultCallback = base::OnceCallback<void(Profile& result)>;

  static std::unique_ptr<CancellableJob> Run(const UserContext& user_context,
                                             ResultCallback on_done) {
    auto handle = base::WrapUnique(new SessionStarter(std::move(on_done)));
    UserSessionManager::GetInstance()->StartSession(
        user_context, UserSessionManager::StartSessionType::kPrimary,
        /*has_auth_cookies=*/false,
        /*has_active_session=*/false,
        /*delegate=*/handle->weak_ptr_factory_.GetWeakPtr());
    return handle;
  }

  SessionStarter(const SessionStarter&) = delete;
  SessionStarter& operator=(const SessionStarter&) = delete;
  ~SessionStarter() override = default;

 private:
  explicit SessionStarter(ResultCallback on_done)
      : on_done_(std::move(on_done)) {}

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override {
    std::move(on_done_).Run(CHECK_DEREF(profile));
  }

  ResultCallback on_done_;
  base::WeakPtrFactory<SessionStarter> weak_ptr_factory_{this};
};

}  // namespace

KioskProfileLoader::KioskProfileLoader(const AccountId& app_account_id,
                                       KioskAppType app_type,
                                       Delegate* delegate)
    : account_id_(app_account_id),
      app_type_(app_type),
      delegate_(delegate),
      failed_mount_attempts_(0) {}

KioskProfileLoader::~KioskProfileLoader() = default;

void KioskProfileLoader::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  login_performer_.reset();
  current_step_ = CheckCryptohome(base::BindOnce(
      [](KioskProfileLoader* self, absl::optional<MountedState> result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
        // Reset `current_step_` to free resources now the job is done.
        self->current_step_.reset();
        if (!result.has_value()) {
          return self->ReportLaunchResult(
              KioskAppLaunchError::Error::kCryptohomedNotRunning);
        }
        switch (result.value()) {
          case MountedState::kNotMounted:
            return self->LoginAsKioskAccount();
          case MountedState::kMounted:
            return self->ReportLaunchResult(
                KioskAppLaunchError::Error::kAlreadyMounted);
        }
      },
      // Safe because `this` owns `current_step_`
      base::Unretained(this)));
}

void KioskProfileLoader::LoginAsKioskAccount() {
  login_performer_ =
      std::make_unique<ChromeLoginPerformer>(this, AuthEventsRecorder::Get());
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

void KioskProfileLoader::OnAuthSuccess(const UserContext& user_context) {
  // LoginPerformer will delete itself.
  login_performer_->set_delegate(nullptr);
  std::ignore = login_performer_.release();

  failed_mount_attempts_ = 0;

  PrepareProfile(user_context);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnOldEncryptionDetected(std::move(user_context));
}

void KioskProfileLoader::PrepareProfile(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ = SessionStarter::Run(
      user_context, base::BindOnce(&KioskProfileLoader::ReportProfileLoaded,
                                   base::Unretained(this)));
}

void KioskProfileLoader::ReportProfileLoaded(Profile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();
  delegate_->OnProfileLoaded(&profile);
}

void KioskProfileLoader::ReportLaunchResult(KioskAppLaunchError::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();

  if (error == KioskAppLaunchError::Error::kCryptohomedNotRunning) {
    SYSLOG(ERROR) << "Cryptohome not available when loading Kiosk profile.";
  } else if (error == KioskAppLaunchError::Error::kAlreadyMounted) {
    SYSLOG(ERROR) << "Cryptohome already mounted when loading Kiosk profile.";
  }

  if (error != KioskAppLaunchError::Error::kNone) {
    delegate_->OnProfileLoadFailed(error);
  }
}

}  // namespace ash
