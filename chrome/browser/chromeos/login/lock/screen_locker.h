// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/login_types.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/security_token_pin_dialog_host_ash_impl.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

class Authenticator;
class ExtendedAuthenticator;
class AuthFailure;
class ViewsScreenLocker;

// ScreenLocker displays the lock UI and takes care of authenticating the user
// and managing a global instance of itself which will be deleted when the
// system is unlocked.
class ScreenLocker : public AuthStatusConsumer,
                     public device::mojom::FingerprintObserver {
 public:
  // Delegate used to send internal state changes back to the UI.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Show the given error message.
    virtual void ShowErrorMessage(int error_msg_id,
                                  HelpAppLauncher::HelpTopic help_topic_id) = 0;

    // Close any displayed error messages.
    virtual void ClearErrors() = 0;

    // Called by ScreenLocker to notify that ash lock animation finishes.
    virtual void OnAshLockAnimationFinished() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  using AuthenticateCallback = base::OnceCallback<void(bool auth_success)>;

  explicit ScreenLocker(const user_manager::UserList& users);

  // Returns the default instance if it has been created.
  static ScreenLocker* default_screen_locker() { return screen_locker_; }

  // Returns true if the lock UI has been confirmed as displayed.
  bool locked() const { return locked_; }

  // Initialize and show the screen locker.
  void Init();

  // AuthStatusConsumer:
  void OnAuthFailure(const chromeos::AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

  // Called when an account password (not PIN/quick unlock) has been used to
  // unlock the device.
  void OnPasswordAuthSuccess(const UserContext& user_context);

  // Disables authentication for the user with |account_id|. Notifies lock
  // screen UI.
  void EnableAuthForUser(const AccountId& account_id);

  // Enables authentication for the user with |account_id|. Notifies lock screen
  // UI. |auth_disabled_data| is used to display information in the UI.
  void DisableAuthForUser(const AccountId& account_id,
                          const ash::AuthDisabledData& auth_disabled_data);

  // Authenticates the user with given |user_context|.
  void Authenticate(const UserContext& user_context,
                    AuthenticateCallback callback);

  // Authenticates the user with given |account_id| using the challenge-response
  // authentication against a security token.
  void AuthenticateWithChallengeResponse(const AccountId& account_id,
                                         AuthenticateCallback callback);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // Exit the chrome, which will sign out the current session.
  void Signout();

  // (Re)enable input field.
  void EnableInput();

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id,
                        bool sign_out_only);

  // Returns delegate that can be used to talk to the view-layer.
  Delegate* delegate() { return delegate_; }

  // Returns the users to authenticate.
  const user_manager::UserList& users() const { return users_; }

  // Allow a AuthStatusConsumer to listen for
  // the same login events that ScreenLocker does.
  void SetLoginStatusConsumer(chromeos::AuthStatusConsumer* consumer);

  // Initialize or uninitialize the ScreenLocker class. It observes
  // SessionManager so that the screen locker accepts lock requests only after a
  // user has logged in.
  static void InitClass();
  static void ShutDownClass();

  // Handles a request from the session manager to show the lock screen.
  static void HandleShowLockScreenRequest();

  // Show the screen locker.
  static void Show();

  // Hide the screen locker.
  static void Hide();

  void RefreshPinAndFingerprintTimeout();

  // Saves sync password hash and salt to user profile prefs based on
  // |user_context|.
  void SaveSyncPasswordHash(const UserContext& user_context);

  // Ruturns true if authentication is enabled on the lock screen for the given
  // user.
  bool IsAuthEnabledForUser(const AccountId& account_id);

  // Change the authenticators; should only be used by tests.
  void SetAuthenticatorsForTesting(
      scoped_refptr<Authenticator> authenticator,
      scoped_refptr<ExtendedAuthenticator> extended_authenticator);

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int32_t percent_complete) override;
  void OnAuthScanDone(
      device::mojom::ScanResult scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

 private:
  friend class base::DeleteHelper<ScreenLocker>;
  friend class ViewsScreenLocker;

  // Track the type of the authentication that the user used to unlock the lock
  // screen.
  // Values correspond to UMA histograms; do not modify, or add or delete other
  // than directly before AUTH_COUNT.
  enum UnlockType {
    AUTH_PASSWORD = 0,
    AUTH_PIN = 1,
    AUTH_FINGERPRINT = 2,
    AUTH_CHALLENGE_RESPONSE = 3,
    AUTH_COUNT
  };

  // State associated with a pending authentication attempt.
  struct AuthState {
    AuthState(AccountId account_id, base::OnceCallback<void(bool)> callback);
    ~AuthState();

    // Account that is being authenticated.
    AccountId account_id;
    // Callback that should be executed the authentication result is available.
    base::OnceCallback<void(bool)> callback;
  };

  ~ScreenLocker() override;

  void OnFingerprintAuthFailure(const user_manager::User& user);

  // Called when the screen lock is ready.
  void ScreenLockReady();

  // Called when screen locker is safe to delete.
  static void ScheduleDeletion();

  // Returns true if |account_id| is found among logged in users.
  bool IsUserLoggedIn(const AccountId& account_id) const;

  // Looks up user in unlock user list.
  const user_manager::User* FindUnlockUser(const AccountId& account_id);

  // Callback to be invoked for ash start lock request. |locked| is true when
  // ash is fully locked and post lock animation finishes. Otherwise, the start
  // lock request is failed.
  void OnStartLockCallback(bool locked);

  // Callback to be invoked when the |challenge_response_auth_keys_loader_|
  // completes building the currently available challenge-response keys. Used
  // only during the challenge-response unlock.
  void OnChallengeResponseKeysPrepared(
      const AccountId& account_id,
      std::vector<ChallengeResponseKey> challenge_response_keys);

  void OnPinAttemptDone(const UserContext& user_context, bool success);

  // Called to continue authentication against cryptohome after the pin login
  // check has completed.
  void ContinueAuthenticate(const UserContext& user_context);

  // Periodically called to see if PIN and fingerprint are still available for
  // use. PIN and fingerprint are disabled after a certain period of time (e.g.
  // 24 hours).
  void MaybeDisablePinAndFingerprintFromTimeout(const std::string& source,
                                                const AccountId& account_id);

  void OnPinCanAuthenticate(const AccountId& account_id, bool can_authenticate);

  // Delegate used to talk to the view.
  Delegate* delegate_ = nullptr;

  // Users that can unlock the device.
  user_manager::UserList users_;

  // Set of users that have authentication disabled on lock screen. Has to be
  // subset of |users_|.
  std::set<AccountId> users_with_disabled_auth_;

  // Used to authenticate the user to unlock.
  scoped_refptr<Authenticator> authenticator_;

  // Used to authenticate the user to unlock supervised users.
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;

  // True if the screen is locked, or false otherwise.  This changes
  // from false to true, but will never change from true to
  // false. Instead, ScreenLocker object gets deleted when unlocked.
  bool locked_ = false;

  // Reference to the single instance of the screen locker object.
  // This is used to make sure there is only one screen locker instance.
  static ScreenLocker* screen_locker_;

  // The time when the screen locker object is created.
  base::Time start_time_ = base::Time::Now();
  // The time when the authentication is started.
  base::Time authentication_start_time_;

  // Delegate to forward all login status events to.
  // Tests can use this to receive login status events.
  AuthStatusConsumer* auth_status_consumer_ = nullptr;

  // Number of bad login attempts in a row.
  int incorrect_passwords_count_ = 0;

  // Type of the last unlock attempt.
  UnlockType unlock_attempt_type_ = AUTH_PASSWORD;

  // State associated with a pending authentication attempt.
  std::unique_ptr<AuthState> pending_auth_state_;

  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state_;

  mojo::Remote<device::mojom::Fingerprint> fp_service_;
  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  // ViewsScreenLocker instance in use.
  std::unique_ptr<ViewsScreenLocker> views_screen_locker_;

  // Password is required every 24 hours in order to use fingerprint unlock.
  // This is used to update fingerprint state when password is required.
  base::OneShotTimer update_fingerprint_state_timer_;

  ChallengeResponseAuthKeysLoader challenge_response_auth_keys_loader_;

  SecurityTokenPinDialogHostAshImpl security_token_pin_dialog_host_ash_impl_;

  base::WeakPtrFactory<ScreenLocker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
