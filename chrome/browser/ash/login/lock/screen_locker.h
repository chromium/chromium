// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/login_types.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/security_token_pin_dialog_host_login_impl.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/ash/input_method_manager.h"

class PrefChangeRegistrar;

namespace ash {

class Authenticator;
class ViewsScreenLocker;

// ScreenLocker displays the lock UI and takes care of authenticating the user
// and managing a global instance of itself which will be deleted when the
// system is unlocked.
class ScreenLocker
    : public AuthStatusConsumer,
      public device::mojom::FingerprintObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool auth_success)>;

  explicit ScreenLocker(const user_manager::UserList& users);

  ScreenLocker(const ScreenLocker&) = delete;
  ScreenLocker& operator=(const ScreenLocker&) = delete;

  // Returns the default instance if it has been created.
  static ScreenLocker* default_screen_locker() { return screen_locker_; }

  // Returns true if the lock UI has been confirmed as displayed.
  bool locked() const { return locked_; }

  // Disables authentication for the user with `account_id`. Notifies lock
  // screen UI. `auth_disabled_data` is used to display information in the UI.
  void TemporarilyDisableAuthForUser(
      const AccountId& account_id,
      const AuthDisabledData& auth_disabled_data);

  // Reenables authentication for the user with `account_id` previously disabled
  // by `TemporarilyDisableAuthForUser`. Notifies lock screen UI.
  void ReenableAuthForUser(const AccountId& account_id);

  // Authenticates the user with given `user_context`.
  void Authenticate(std::unique_ptr<UserContext> user_context,
                    AuthenticateCallback callback);

  // Authenticates the user with given `account_id` using the challenge-response
  // authentication against a security token.
  void AuthenticateWithChallengeResponse(const AccountId& account_id,
                                         AuthenticateCallback callback);

  // Returns the users to show on the lock screen UI. Will be a subset of
  // `users()`.
  user_manager::UserList GetUsersToShow() const;

  // Allow a AuthStatusConsumer to listen for
  // the same login events that ScreenLocker does.
  void SetLoginStatusConsumer(AuthStatusConsumer* consumer);

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

  // Returns true if authentication is enabled on the lock screen for the given
  // user.
  bool IsAuthTemporarilyDisabledForUser(const AccountId& account_id);

  static void SetClocksForTesting(const base::Clock* clock,
                                  const base::TickClock* tick_clock);

 private:
  friend class base::DeleteHelper<ScreenLocker>;
  friend class ScreenLockerTester;

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

  // Initialize and show the screen locker.
  void Init();

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnStatusChanged(device::mojom::BiometricsManagerStatus status) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int32_t percent_complete) override;
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // If the unlock animation was aborted (for instance, as a result of
  // pressing the power button during the unlock animatoin), we  reset
  // the state of UI elements (such as LoginAuthUserView::FingerprintView)
  // which might have been altered as a result of a successful authentication
  // attempt.
  void ResetToLockedState();

  // If the unlock animation was not aborted, changes session state to
  // active and schedules `ScreenLocker` deletion.
  static void OnUnlockAnimationFinished(bool aborted);

  // TODO(b/271261286): we should probably not call it anymore
  void RefreshPinAndFingerprintTimeout();

  // Change the authenticators; should only be used by tests.
  void SetAuthenticatorsForTesting(scoped_refptr<Authenticator> authenticator);

  void OnFingerprintAuthFailure(const user_manager::User& user);

  void StartFingerprintAuthSession(const user_manager::User* primary_user);

  // Called when the screen lock is ready.
  void ScreenLockReady();

  // Called when screen locker is safe to delete.
  static void ScheduleDeletion();

  // Returns true if `account_id` is found among logged in users.
  bool IsUserLoggedIn(const AccountId& account_id) const;

  // Looks up user in unlock user list.
  const user_manager::User* FindUnlockUser(const AccountId& account_id);

  // Callback to be invoked for ash start lock request. `locked` is true when
  // ash is fully locked and post lock animation finishes. Otherwise, the start
  // lock request is failed.
  void OnStartLockCallback(bool locked);

  // Callback to be invoked when the `challenge_response_auth_keys_loader_`
  // completes building the currently available challenge-response keys. Used
  // only during the challenge-response unlock.
  void OnChallengeResponseKeysPrepared(
      const AccountId& account_id,
      std::vector<ChallengeResponseKey> challenge_response_keys);

  void OnPinAttemptDone(std::unique_ptr<UserContext>,
                        std::optional<AuthenticationError>);

  // Called to select the appropriate Authenticator and perform unlock
  // operation.
  void AttemptUnlock(std::unique_ptr<UserContext> user_context);

  // Called to continue authentication against cryptohome after the pin login
  // check has completed.
  void ContinueAuthenticate(std::unique_ptr<UserContext> user_context);

  // Periodically called to see if PIN and fingerprint are still available for
  // use. PIN and fingerprint are disabled after a certain period of time (e.g.
  // 24 hours).
  void MaybeDisablePinAndFingerprintFromTimeout(const std::string& source,
                                                const AccountId& account_id);

  void OnPinCanAuthenticate(const AccountId& account_id,
                            bool can_authenticate,
                            cryptohome::PinLockAvailability available_at);

  void UpdateFingerprintStateForUser(const user_manager::User* user);

  void OnEndFingerprintAuthSession(bool success);

  // Helper to transform internal enum UnlockType to
  // session_manager::UnlockType, used by the reporting team to report
  // lock/unlock events.
  session_manager::UnlockType TransformUnlockType();

  // Users that can unlock the device.
  user_manager::UserList users_;

  // Set of users that have authentication temporarily disabled on lock screen
  // (for example because of too much screen time). Has to be subset of
  // `users_`.
  std::set<AccountId> users_with_temporarily_disabled_auth_;

  // Used to authenticate the user to unlock.
  scoped_refptr<Authenticator> authenticator_;

  // True if the screen is locked, or false otherwise.  This changes
  // from false to true, but will never change from true to
  // false. Instead, ScreenLocker object gets deleted when unlocked.
  bool locked_ = false;

  // True if the unlock process has started, or false otherwise.  This changes
  // from false to true, but will only change from true to false when unlock is
  // aborted. Otherwise, ScreenLocker object gets deleted when unlocked.
  bool unlock_started_ = false;

  // Reference to the single instance of the screen locker object.
  // This is used to make sure there is only one screen locker instance.
  static ScreenLocker* screen_locker_;

  // The time when the screen locker object is created.
  base::Time start_time_ = base::Time::Now();
  // The time when the authentication is started.
  base::Time authentication_start_time_;

  // Delegate to forward all login status events to.
  // Tests can use this to receive login status events.
  raw_ptr<AuthStatusConsumer> auth_status_consumer_ = nullptr;

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
  std::unique_ptr<base::WallClockTimer> update_fingerprint_state_timer_;

  ChallengeResponseAuthKeysLoader challenge_response_auth_keys_loader_;

  SecurityTokenPinDialogHostLoginImpl
      security_token_pin_dialog_host_login_impl_;

  std::unique_ptr<PrefChangeRegistrar> fingerprint_pref_change_registrar_;

  base::WeakPtrFactory<ScreenLocker> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_H_
