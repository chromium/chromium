// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "components/account_id/account_id.h"

class Profile;

namespace ash {

class AuthFailure;
enum class KioskAppType;
class UserContext;

// KioskProfileLoader loads a special profile for a given app. It first
// attempts to login for the app's generated user id. If the login is
// successful, it prepares app profile then calls the delegate.
class KioskProfileLoader : public LoginPerformer::Delegate,
                           public UserSessionManagerDelegate {
 public:
  class Delegate {
   public:
    virtual void OnProfileLoaded(Profile* profile) = 0;
    virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) = 0;
    virtual void OnOldEncryptionDetected(
        std::unique_ptr<UserContext> user_context) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  KioskProfileLoader(const AccountId& app_account_id,
                     KioskAppType app_type,
                     Delegate* delegate);
  KioskProfileLoader(const KioskProfileLoader&) = delete;
  KioskProfileLoader& operator=(const KioskProfileLoader&) = delete;
  ~KioskProfileLoader() override;

  // Starts profile load. Calls delegate on success or failure.
  void Start();

 private:
  class CryptohomedChecker;

  void LoginAsKioskAccount();
  void ReportLaunchResult(KioskAppLaunchError::Error error);

  // LoginPerformer::Delegate overrides:
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnAuthFailure(const AuthFailure& error) override;
  void AllowlistCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void OnOldEncryptionDetected(std::unique_ptr<UserContext> user_context,
                               bool has_incomplete_migration) override;

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  const AccountId account_id_;
  const KioskAppType app_type_;
  raw_ptr<Delegate, ExperimentalAsh> delegate_;
  int failed_mount_attempts_;
  std::unique_ptr<CryptohomedChecker> cryptohomed_checker_;
  std::unique_ptr<LoginPerformer> login_performer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
