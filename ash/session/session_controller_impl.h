// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_CONTROLLER_IMPL_H_
#define ASH_SESSION_SESSION_CONTROLLER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_activation_observer_holder.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

class AccountId;
class PrefRegistrySimple;
class PrefService;

namespace ash {

class FullscreenController;
class ScopedScreenLockBlocker;
class SessionControllerClient;
class SessionObserver;
class TestSessionControllerClient;

// Implements mojom::SessionController to cache session related info such as
// session state, meta data about user sessions to support synchronous
// queries for ash.
class ASH_EXPORT SessionControllerImpl : public SessionController {
 public:
  using UserSessions = std::vector<std::unique_ptr<UserSession>>;

  SessionControllerImpl();

  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

  ~SessionControllerImpl() override;

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  base::TimeDelta session_length_limit() const { return session_length_limit_; }
  base::Time session_start_time() const { return session_start_time_; }

  // Returns the ash notion of login status.
  // NOTE: Prefer GetSessionState() in new code because the concept of
  // SessionState more closes matches the state in chrome.
  LoginStatus login_status() const { return login_status_; }

  // Returns the number of signed in users. If 0 is returned, there is either
  // no session in progress or no active user.
  int NumberOfLoggedInUsers() const;

  // Returns the active account. If no users are logged in this returns an empty
  // AccountId.
  AccountId GetActiveAccountId() const;

  // Gets the policy of adding a user session to ash.
  AddUserSessionPolicy GetAddUserPolicy() const;

  // Returns true if the active user account is under any policy management.
  // NOTE: This function should only be called if the active account exists.
  bool IsActiveAccountManaged() const;

  // Returns |true| if the session has been fully started for the active user.
  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this method starts returning |true| is the browser
  // startup complete and both profile and UI are fully available.
  bool IsActiveUserSessionStarted() const;

  // Returns true if the screen can be locked.
  bool CanLockScreen() const;

  // Returns true if the screen should be locked automatically when the screen
  // is turned off or the system is suspended.
  bool ShouldLockScreenAutomatically() const;

  // Returns true if the session is in a kiosk-like mode running a single app.
  bool IsRunningInAppMode() const;

  // Returns true if the current session is a demo session for Demo Mode.
  bool IsDemoSession() const;

  // Returns true if user session blocked by some overlying UI. It can be
  // login screen, lock screen or screen for adding users into multi-profile
  // session.
  bool IsUserSessionBlocked() const;

  // Convenience function that returns true if session state is LOGIN_SECONDARY.
  bool IsInSecondaryLoginScreen() const;

  // Returns true if the settings icon should be enabled in the system tray.
  bool ShouldEnableSettings() const;

  // Returns true if the notification tray should appear.
  bool ShouldShowNotificationTray() const;

  // Gets the ash session state.
  session_manager::SessionState GetSessionState() const;

  // Gets the user sessions in LRU order with the active session being first.
  const UserSessions& GetUserSessions() const;

  // Convenience helper to get the user session at a given index. Returns
  // nullptr if no user session is found for the index.
  const UserSession* GetUserSession(UserIndex index) const;

  // Convenience helper to get the user session with the given account id.
  // Returns nullptr if no user session is found for the account id.
  const UserSession* GetUserSessionByAccountId(
      const AccountId& account_id) const;

  // Gets the primary user session.
  const UserSession* GetPrimaryUserSession() const;

  // Returns true if the current user is a child account.
  bool IsUserChild() const;

  // Returns true if the current user is a guest account.
  bool IsUserGuest() const;

  // Returns true if the current user is a public account.
  bool IsUserPublicAccount() const;

  // Returns the type of the current user, or empty if there is no current user
  // logged in.
  std::optional<user_manager::UserType> GetUserType() const;

  // Returns true if the current user is the primary user in a multi-profile
  // scenario. This always return true if there is only one user logged in.
  bool IsUserPrimary() const;

  // Returns true if the current user has the profile newly created on the
  // device (i.e. first time login on the device).
  bool IsUserFirstLogin() const;

  // Returns true if should display managed icon for current session,
  // and false otherwise.
  bool ShouldDisplayManagedUI() const;

  // Locks the screen. The locking happens asynchronously.
  void LockScreen();

  // Hides the lock screen.
  void HideLockScreen();

  // Requests signing out all users, ending the current session.
  void RequestSignOut();

  // Requests a system restart to apply an OS update.
  void RequestRestartForUpdate();

  // Attempts to restart the chrome browser.
  void AttemptRestartChrome();

  // Switches to another active user with `account_id` (if that user has
  // already signed in).
  void SwitchActiveUser(const AccountId& account_id);

  // Switches the active user to the next or previous user, with the same
  // ordering as user sessions are created.
  void CycleActiveUser(CycleUserDirection direction);

  // Show the multi-profile login UI to add another user to this session.
  void ShowMultiProfileLogin();

  // Returns the PrefService used at the signin screen, which is tied to an
  // incognito profile in chrome and is valid until the browser exits.
  PrefService* GetSigninScreenPrefService() const;

  // Returns the PrefService for `account_id` or null if one does not exist.
  PrefService* GetUserPrefServiceForUser(const AccountId& account_id) const;

  // Returns the profile path for `account_id` or empty if one does not exist.
  base::FilePath GetProfilePath(const AccountId& account_id) const;

  // Returns a tuple of whether
  // <IsVcBackgroundSupported, IsVcBackgroundAllowedByEnterprise>.
  std::tuple<bool, bool> IsEligibleForSeaPen(const AccountId& account_id) const;

  // Returns the PrefService for the primary user or null if no user is signed
  // in or the PrefService connection hasn't been established.
  PrefService* GetPrimaryUserPrefService() const;

  // Returns the PrefService for the last active user that had one or null if no
  // PrefService connection has been successfully established.
  PrefService* GetLastActiveUserPrefService() const;

  // Before login returns the signin screen profile prefs. After login returns
  // the active user profile prefs. Returns null early during startup.
  PrefService* GetActivePrefService() const;

  // Returns an object of `ScopedScreenLockBlocker`.
  // `CanLockScreen()` returns false while there is one or more living object.
  std::unique_ptr<ScopedScreenLockBlocker> GetScopedScreenLockBlocker();

  // SessionController:
  void SetClient(SessionControllerClient* client) override;
  void SetSessionInfo(const SessionInfo& info) override;
  void UpdateUserSession(const UserSession& user_session) override;
  void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_order) override;
  void PrepareForLock(PrepareForLockCallback callback) override;
  void StartLock(StartLockCallback callback) override;
  void NotifyChromeLockAnimationsComplete() override;
  void RunUnlockAnimation(RunUnlockAnimationCallback callback) override;
  void NotifyChromeTerminating() override;
  void SetSessionLengthLimit(base::TimeDelta length_limit,
                             base::Time start_time) override;
  void CanSwitchActiveUser(CanSwitchActiveUserCallback callback) override;
  void ShowMultiprofilesIntroDialog(
      ShowMultiprofilesIntroDialogCallback callback) override;
  void ShowTeleportWarningDialog(
      ShowTeleportWarningDialogCallback callback) override;
  void ShowMultiprofilesSessionAbortedDialog(
      const std::string& user_email) override;
  void AddSessionActivationObserverForAccountId(
      const AccountId& account_id,
      SessionActivationObserver* observer) override;
  void RemoveSessionActivationObserverForAccountId(
      const AccountId& account_id,
      SessionActivationObserver* observer) override;
  void AddObserver(SessionObserver* observer) override;
  void RemoveObserver(SessionObserver* observer) override;
  bool IsScreenLocked() const override;
  std::optional<int> GetExistingUsersCount() const override;
  void NotifyFirstSessionReady() override;
  void NotifyUserToBeRemoved(const AccountId& account_id) override;

  // Test helpers.
  void ClearUserSessionsForTest();

 private:
  friend class TestSessionControllerClient;

  // Provides the implementation of `ScopedScreenLockBlocker`.
  // Defined as a private class of `SessionControllerImpl` so that it can call
  // `RemoveScopedScreenLockBlocker()` in its dtor.
  class ScopedScreenLockBlockerImpl;

  // Marks the session as a demo session for Demo Mode.
  void SetIsDemoSession();
  void SetSessionState(session_manager::SessionState state);
  void AddUserSession(const UserSession& user_session);

  // Calculate login status based on session state and active user session.
  LoginStatus CalculateLoginStatus() const;

  // Helper that returns login status when the session state is ACTIVE.
  LoginStatus CalculateLoginStatusForActiveSession() const;

  // Update the |login_status_| and notify observers.
  void UpdateLoginStatus();

  // Used as lock screen displayed callback of LockStateController and invoked
  // when post lock animation finishes and ash is fully locked. It would then
  // run |start_lock_callback_| to indicate ash is locked successfully.
  void OnLockAnimationFinished();

  // Ensure that the sign-in screen PrefService is obtained.
  void EnsureSigninScreenPrefService();

  void OnSigninScreenPrefServiceInitialized(PrefService* pref_service);

  void OnProfilePrefServiceInitialized(const AccountId& account_id,
                                       PrefService* pref_service);

  // Notifies observers that the active user pref service changed only if the
  // signin profile pref service has been connected and observers were notified
  // via OnSigninScreenPrefServiceInitialized(). Otherwise, defer the
  // notification until that happens.
  void MaybeNotifyOnActiveUserPrefServiceChanged();

  // Called when IsUserSessionBlocked() becomes true. If there isn't an active
  // window, tries to activate one.
  void EnsureActiveWindowAfterUnblockingUserSession();

  // Called when an object of `ScopedScreenLockBlockerImpl` is destroyed.
  void RemoveScopedScreenLockBlocker();

  // Client interface to session manager code (chrome).
  raw_ptr<SessionControllerClient> client_ = nullptr;

  // Cached session info.
  bool can_lock_ = false;
  bool should_lock_screen_automatically_ = false;
  bool is_running_in_app_mode_ = false;
  bool is_demo_session_ = false;
  AddUserSessionPolicy add_user_session_policy_ = AddUserSessionPolicy::ALLOWED;
  session_manager::SessionState state_ = session_manager::SessionState::UNKNOWN;

  // Cached user session info sorted by the order from SetUserSessionOrder.
  // Currently the session manager code (chrome) sets a LRU order with the
  // active session being the first.
  UserSessions user_sessions_;

  // The user session id of the current active user session. User session id
  // is managed by session manager code, starting at 1. 0u is an invalid id
  // to detect first active user session.
  uint32_t active_session_id_ = 0u;

  // The user session id of the primary user session. The primary user session
  // is the very first user session of the current ash session.
  uint32_t primary_session_id_ = 0u;

  // Last known login status. Used to track login status changes.
  LoginStatus login_status_ = LoginStatus::NOT_LOGGED_IN;

  // Whether unlocking is in progress. The flag is set when the pre-unlock
  // animation starts and reset when session state is no longer LOCKED.
  bool is_unlocking_ = false;

  // Pending callback for the StartLock request.
  base::OnceCallback<void(bool)> start_lock_callback_;

  // The session length limit; set to zero if there is no limit.
  base::TimeDelta session_length_limit_;

  // The session start time, set at login or on the first user activity; set to
  // null if there is no session length limit. This value is also stored in a
  // pref in case of a crash during the session.
  base::Time session_start_time_;

  // Set to true if the active user's pref is received before the signin prefs.
  // This is so that we can guarantee that observers are notified with
  // OnActiveUserPrefServiceChanged() after
  // OnSigninScreenPrefServiceInitialized().
  bool on_active_user_prefs_changed_notify_deferred_ = false;

  base::ObserverList<SessionObserver> observers_;

  SessionActivationObserverHolder session_activation_observer_holder_;

  bool signin_screen_prefs_obtained_ = false;

  raw_ptr<PrefService> last_active_user_prefs_ = nullptr;

  std::unique_ptr<FullscreenController> fullscreen_controller_;

  int scoped_screen_lock_blocker_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SessionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_CONTROLLER_IMPL_H_
