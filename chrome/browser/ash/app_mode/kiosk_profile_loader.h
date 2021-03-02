// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chromeos/login/auth/login_performer.h"
// TODO(https://crbug.com/1164001): remove when migrated to ash/components/.
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"

class Profile;

namespace ash {

enum class KioskAppType;

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
    virtual void OnOldEncryptionDetected(const UserContext& user_context) = 0;

   protected:
    virtual ~Delegate() {}
  };

  KioskProfileLoader(const AccountId& app_account_id,
                     KioskAppType app_type,
                     bool use_guest_mount,
                     Delegate* delegate);

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
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  const AccountId account_id_;
  const KioskAppType app_type_;
  bool use_guest_mount_;
  Delegate* delegate_;
  int failed_mount_attempts_;
  std::unique_ptr<CryptohomedChecker> cryptohomed_checker_;
  std::unique_ptr<LoginPerformer> login_performer_;

  DISALLOW_COPY_AND_ASSIGN(KioskProfileLoader);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// source code migration is finished.
namespace chromeos {
using ::ash::KioskProfileLoader;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
