// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/reauth_reason.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace user_manager {
class User;
}

namespace ash {

class AuthSessionAuthenticator;
class AuthenticationError;
class PasswordUpdateFlow;

using PasswordChangedCallback = ::base::RepeatingClosure;

// Manages online re-auth requests triggered by online signin policy or by
// checking validity of SAML_PASSWORD sync token and sends user through online
// signin flow on the lock screen.
// This object is created by LockScreenReauthManagerFactory per primary user in
// active session.
class LockScreenReauthManager : public KeyedService,
                                public session_manager::SessionManagerObserver,
                                public AuthStatusConsumer {
 public:
  explicit LockScreenReauthManager(Profile* primary_profile);
  ~LockScreenReauthManager() override;

  LockScreenReauthManager(const LockScreenReauthManager&) = delete;
  LockScreenReauthManager& operator=(const LockScreenReauthManager&) = delete;

  // Checks if lockscreen re-authentication is enabled for the given profile.
  // Note that it can be changed in session. Driven by the policy
  // LockScreenReauthenticationEnabled.
  bool ShouldPasswordSyncTriggerReauth();

  // Sets online re-auth on lock flag and changes the UI to online
  // re-auth when called on the lock screen.
  void MaybeForceReauthOnLockScreen(ReauthReason reauth_reason);

  // Set special clock for testing.
  void SetClockForTesting(const base::Clock* clock);

  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver::
  void OnSessionStateChanged() override;

  // Checks user's credentials.
  // In case of success, OnAuthSuccess will be triggered.
  // |user_context| has the user's credentials that needs to be checked.
  // |callback| is used in case the user's password does not match the
  // password that is used in encrypting his data by cryptohome
  void CheckCredentials(const UserContext& user_context,
                        PasswordChangedCallback callback);

  // Change the user's old password with the new one which is used to
  // verify the user on an IdP.
  void UpdateUserPassword(const std::string& old_password);

  // AuthStatusConsumer:
  // Shows password changed dialog.
  void OnAuthFailure(const AuthFailure& error) override;

  // Unlocks the screen if primary user successfully verified the password
  // with an IdP.
  void OnAuthSuccess(const UserContext& user_context) override;

  // Resets `is_reauth_required_by_saml_token_mismatch_` flag.
  void ResetReauthRequiredBySamlTokenDismatch();

 private:
  // Triggered when cookies are transferred to the user profile
  void OnCookiesTransferred();

  // Forces the online reauth flow on the lock screen.
  void ForceOnlineReauth();

  // Reset the online reauth, triggered after successful online authentication.
  void ResetOnlineReauth();

  void OnPasswordUpdateSuccess(std::unique_ptr<UserContext> user_context);
  void OnPasswordUpdateFailure(std::unique_ptr<UserContext> user_context,
                               AuthenticationError error);

  // Send the reason(s) why a user is required to reauthenticate online in the
  // lock screen to UMA.
  void SendLockscreenReauthReason();

  const raw_ptr<Profile> primary_profile_;
  const raw_ptr<const user_manager::User, DanglingUntriaged> primary_user_;
  UserContext user_context_;
  raw_ptr<const base::Clock> clock_;
  raw_ptr<proximity_auth::ScreenlockBridge> screenlock_bridge_;

  // Used to authenticate the user.
  scoped_refptr<AuthSessionAuthenticator> auth_session_authenticator_;
  std::unique_ptr<PasswordUpdateFlow> password_update_flow_;

  InSessionPasswordSyncManager in_session_password_sync_manager_;

  PasswordChangedCallback password_changed_callback_;

  friend class LockScreenReauthManagerTest;
  friend class InSessionPasswordSyncManagerTest;
  friend class LockScreenReauthManagerFactory;

  bool is_reauth_required_by_gaia_time_limit_policy_ = false;
  bool is_reauth_required_by_saml_time_limit_policy_ = false;
  bool is_reauth_required_by_saml_token_mismatch_ = false;

  base::WeakPtrFactory<LockScreenReauthManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_H_
