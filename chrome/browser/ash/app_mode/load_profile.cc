// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/load_profile.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/retry_runner.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

namespace ash::kiosk {

namespace {

bool IsTestOrLinuxChromeOS() {
  // This code should only run in Chrome OS, so not `IsRunningOnChromeOS()`
  // means it's either a test or linux-chromeos.
  return !base::SysInfo::IsRunningOnChromeOS();
}

KioskAppLaunchError::Error LoginFailureToKioskLaunchError(
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
      LOG(ERROR) << "KIOSK launch error because of AuthFailure::FailureReason: "
                 << error.reason();
      return KioskAppLaunchError::Error::kUnableToMount;
  }
}

CryptohomeMountState ToResult(
    std::optional<user_data_auth::IsMountedReply> reply) {
  if (!reply.has_value()) {
    return CryptohomeMountState::kServiceUnavailable;
  }
  if (IsTestOrLinuxChromeOS()) {
    // In tests and in linux-chromeos there is no real cryptohome, and the fake
    // one always replies with `is_mounted()` true. We override the reply so
    // Kiosk login can proceed.
    return CryptohomeMountState::kNotMounted;
  }
  return reply->is_mounted() ? CryptohomeMountState::kMounted
                             : CryptohomeMountState::kNotMounted;
}

void CheckCryptohomeMountState(CryptohomeMountStateCallback on_done) {
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      [](CryptohomeMountStateCallback on_done, bool service_is_ready) {
        if (!service_is_ready || !UserDataAuthClient::Get()) {
          return std::move(on_done).Run(
              CryptohomeMountState::kServiceUnavailable);
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
  return RunUpToNTimes<CryptohomeMountState>(
      /*n=*/5,
      /*job=*/base::BindRepeating(&CheckCryptohomeMountState),
      /*should_retry=*/
      base::BindRepeating([](const CryptohomeMountState& result) {
        return result == CryptohomeMountState::kServiceUnavailable;
      }),
      std::move(on_done));
}

class SigninPerformer : public LoginPerformer::Delegate, public CancellableJob {
 public:
  static std::unique_ptr<CancellableJob> Run(
      KioskAppType app_type,
      AccountId account_id,
      PerformSigninResultCallback on_done) {
    auto handle = base::WrapUnique(new SigninPerformer(std::move(on_done)));

    switch (app_type) {
      case KioskAppType::kChromeApp:
        handle->login_performer_->LoginAsKioskAccount(account_id);
        break;
      case KioskAppType::kWebApp:
        handle->login_performer_->LoginAsWebKioskAccount(account_id);
        break;
      case KioskAppType::kIsolatedWebApp:
        handle->login_performer_->LoginAsIwaKioskAccount(account_id);
        break;
    }

    return handle;
  }

  SigninPerformer(const SigninPerformer&) = delete;
  SigninPerformer& operator=(const SigninPerformer&) = delete;
  ~SigninPerformer() override = default;

 private:
  explicit SigninPerformer(PerformSigninResultCallback on_done)
      : on_done_(std::move(on_done)),
        login_performer_(std::make_unique<ChromeLoginPerformer>(
            this,
            AuthEventsRecorder::Get())) {}

  // LoginPerformer::Delegate overrides:
  void OnAuthSuccess(const UserContext& user_context) override {
    // `LoginPerformer` manages its own lifecycle on success, release ownership.
    login_performer_->set_delegate(nullptr);
    std::ignore = login_performer_.release();

    std::move(on_done_).Run(user_context);
  }
  void OnAuthFailure(const AuthFailure& auth_error) override {
    KioskAppLaunchError::SaveCryptohomeFailure(auth_error);
    std::move(on_done_).Run(base::unexpected(auth_error));
  }
  void PolicyLoadFailed() override {
    std::move(on_done_).Run(
        base::unexpected(PerformSigninError::kPolicyLoadFailed));
  }
  void AllowlistCheckFailed(const std::string& email) override {
    std::move(on_done_).Run(
        base::unexpected(PerformSigninError::kAllowlistCheckFailed));
  }
  void OnOldEncryptionDetected(std::unique_ptr<UserContext> user_context,
                               bool has_incomplete_migration) override {
    NOTREACHED();
  }

  PerformSigninResultCallback on_done_;
  std::unique_ptr<LoginPerformer> login_performer_;
};

bool IsRetriableError(const PerformSigninResult& result) {
  if (!result.has_value() &&
      std::holds_alternative<AuthFailure>(result.error())) {
    // Signal a retriable error if the cryptohome mount failed due to
    // corruption of the on-disk state. We always ask to "create" cryptohome
    // and the corrupted one was deleted under the hood.
    return std::get<AuthFailure>(result.error()).reason() ==
           AuthFailure::UNRECOVERABLE_CRYPTOHOME;
  }
  return false;
}

std::unique_ptr<CancellableJob> Signin(KioskAppType app_type,
                                       AccountId account_id,
                                       PerformSigninResultCallback on_done) {
  return RunUpToNTimes<PerformSigninResult>(
      /*n=*/3,
      /*job=*/
      base::BindRepeating(
          [](KioskAppType app_type, AccountId account_id,
             RetryResultCallback<PerformSigninResult> on_result) {
            return SigninPerformer::Run(app_type, account_id,
                                        std::move(on_result));
          },
          app_type, account_id),
      /*should_retry=*/base::BindRepeating(&IsRetriableError),
      /*on_done=*/
      std::move(on_done));
}

KioskAppLaunchError::Error SigninErrorToKioskLaunchError(
    PerformSigninError error) {
  switch (error) {
    case PerformSigninError::kPolicyLoadFailed:
      return KioskAppLaunchError::Error::kPolicyLoadFailed;
    case PerformSigninError::kAllowlistCheckFailed:
      return KioskAppLaunchError::Error::kUserNotAllowlisted;
  }
}

class SessionStarter : public CancellableJob,
                       public UserSessionManagerDelegate {
 public:
  static std::unique_ptr<CancellableJob> Run(
      const UserContext& user_context,
      StartSessionResultCallback on_done) {
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
  explicit SessionStarter(StartSessionResultCallback on_done)
      : on_done_(std::move(on_done)) {}

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override {
    std::move(on_done_).Run(CHECK_DEREF(profile));
  }

  base::WeakPtr<UserSessionManagerDelegate> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  StartSessionResultCallback on_done_;
  base::WeakPtrFactory<SessionStarter> weak_ptr_factory_{this};
};

void LogErrorToSyslog(KioskAppLaunchError::Error error) {
  switch (error) {
    case KioskAppLaunchError::Error::kCryptohomedNotRunning:
      SYSLOG(ERROR) << "Cryptohome not available when loading Kiosk profile.";
      break;
    case KioskAppLaunchError::Error::kAlreadyMounted:
      SYSLOG(ERROR) << "Cryptohome already mounted when loading Kiosk profile.";
      break;
    case KioskAppLaunchError::Error::kUserNotAllowlisted:
      SYSLOG(ERROR) << "LoginPerformer disallowed Kiosk user sign in.";
      break;
    default:
      SYSLOG(ERROR) << "Unexpected error " << (int)error;
  }
}

// Helper class that implements the functionality of `LoadProfile`.
// See docs on that function for more information.
class ProfileLoader : public CancellableJob {
 public:
  static std::unique_ptr<CancellableJob> Run(
      const AccountId& app_account_id,
      KioskAppType app_type,
      CheckCryptohomeCallback check_cryptohome,
      PerformSigninCallback perform_signin,
      StartSessionCallback start_session,
      LoadProfileResultCallback on_done);

  ProfileLoader(const ProfileLoader&) = delete;
  ProfileLoader& operator=(const ProfileLoader&) = delete;
  ~ProfileLoader() override;

 private:
  ProfileLoader(const AccountId& app_account_id,
                KioskAppType app_type,
                CheckCryptohomeCallback check_cryptohome,
                PerformSigninCallback perform_signin,
                StartSessionCallback start_session,
                LoadProfileResultCallback on_done);

  void CheckCryptohomeIsNotMounted();
  void DidCheckCryptohomeIsNotMounted(CryptohomeMountState result);
  void LoginAsKioskAccount();
  void DidLoginAsKioskAccount(PerformSigninResult result);
  void PrepareProfile(const UserContext& user_context);
  void ReturnSuccess(Profile& profile);
  void ReturnError(KioskAppLaunchError::Error result);

  const AccountId account_id_;
  const KioskAppType app_type_;

  // `current_step_` is a handle to the job currently being executed. The
  // possible steps are listed in the callbacks below.
  std::unique_ptr<CancellableJob> current_step_
      GUARDED_BY_CONTEXT(sequence_checker_);
  CheckCryptohomeCallback check_cryptohome_;
  PerformSigninCallback perform_signin_;
  StartSessionCallback start_session_;

  LoadProfileResultCallback on_done_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ProfileLoader> weak_ptr_factory_{this};
};

std::unique_ptr<CancellableJob> ProfileLoader::Run(
    const AccountId& app_account_id,
    KioskAppType app_type,
    CheckCryptohomeCallback check_cryptohome,
    PerformSigninCallback perform_signin,
    StartSessionCallback start_session,
    LoadProfileResultCallback on_done) {
  auto loader = base::WrapUnique(new ProfileLoader(
      app_account_id, app_type, std::move(check_cryptohome),
      std::move(perform_signin), std::move(start_session), std::move(on_done)));
  loader->CheckCryptohomeIsNotMounted();
  return loader;
}

ProfileLoader::ProfileLoader(const AccountId& app_account_id,
                             KioskAppType app_type,
                             CheckCryptohomeCallback check_cryptohome,
                             PerformSigninCallback perform_signin,
                             StartSessionCallback start_session,
                             LoadProfileResultCallback on_done)
    : account_id_(app_account_id),
      app_type_(app_type),
      check_cryptohome_(std::move(check_cryptohome)),
      perform_signin_(std::move(perform_signin)),
      start_session_(std::move(start_session)),
      on_done_(std::move(on_done)) {}

ProfileLoader::~ProfileLoader() = default;

void ProfileLoader::CheckCryptohomeIsNotMounted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ =
      std::move(check_cryptohome_)
          .Run(base::BindOnce(&ProfileLoader::DidCheckCryptohomeIsNotMounted,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ProfileLoader::DidCheckCryptohomeIsNotMounted(
    CryptohomeMountState result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (result) {
    case CryptohomeMountState::kNotMounted:
      return LoginAsKioskAccount();
    case CryptohomeMountState::kMounted:
      return ReturnError(KioskAppLaunchError::Error::kAlreadyMounted);
    case CryptohomeMountState::kServiceUnavailable:
      return ReturnError(KioskAppLaunchError::Error::kCryptohomedNotRunning);
  }
}

void ProfileLoader::LoginAsKioskAccount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ =
      std::move(perform_signin_)
          .Run(app_type_, account_id_,
               /*on_done=*/
               base::BindOnce(&ProfileLoader::DidLoginAsKioskAccount,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ProfileLoader::DidLoginAsKioskAccount(PerformSigninResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.has_value()) {
    return PrepareProfile(result.value());
  } else if (auto* error = std::get_if<PerformSigninError>(&result.error())) {
    return ReturnError(SigninErrorToKioskLaunchError(*error));
  } else if (auto* auth_failure = std::get_if<AuthFailure>(&result.error())) {
    return ReturnError(LoginFailureToKioskLaunchError(*auth_failure));
  }
  NOTREACHED();
}

void ProfileLoader::PrepareProfile(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ = std::move(start_session_)
                      .Run(user_context,
                           /*on_done=*/
                           base::BindOnce(&ProfileLoader::ReturnSuccess,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ProfileLoader::ReturnSuccess(Profile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();
  std::move(on_done_).Run(&profile);
}

void ProfileLoader::ReturnError(KioskAppLaunchError::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();
  LogErrorToSyslog(result);
  std::move(on_done_).Run(base::unexpected(std::move(result)));
}

}  // namespace

std::unique_ptr<CancellableJob> LoadProfile(const AccountId& app_account_id,
                                            KioskAppType app_type,
                                            LoadProfileResultCallback on_done) {
  return LoadProfileWithCallbacks(
      app_account_id, app_type, base::BindOnce(&CheckCryptohome),
      base::BindOnce(&Signin), base::BindOnce(&SessionStarter::Run),
      std::move(on_done));
}

std::unique_ptr<CancellableJob> LoadProfileWithCallbacks(
    const AccountId& app_account_id,
    KioskAppType app_type,
    CheckCryptohomeCallback check_cryptohome,
    PerformSigninCallback perform_signin,
    StartSessionCallback start_session,
    LoadProfileResultCallback on_done) {
  return ProfileLoader::Run(
      app_account_id, app_type, std::move(check_cryptohome),
      std::move(perform_signin), std::move(start_session), std::move(on_done));
}

}  // namespace ash::kiosk
