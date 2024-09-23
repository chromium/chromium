// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_MANAGER_H_

#include <bitset>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/net/always_on_vpn_manager.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/child_accounts/child_policy_observer.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/net/xdr_manager.h"
#include "chrome/browser/ash/release_notes/release_notes_notification.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "ui/base/ime/ash/input_method_manager.h"

class AccountId;
class GURL;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace user_manager {
class User;
class KnownUser;
}  // namespace user_manager

namespace ash {

class AuthStatusConsumer;
class OnboardingUserActivityCounter;
class AuthenticatorBuilder;
class TokenHandleFetcher;
class EolNotification;
class InputEventsBlocker;
class U2FNotification;

namespace test {
class UserSessionManagerTestApi;
}  // namespace test

class UserSessionManagerDelegate {
 public:
  // Called after profile is loaded and prepared for the session.
  // `browser_launched` will be true is browser has been launched, otherwise
  // it will return false and client is responsible on launching browser.
  virtual void OnProfilePrepared(Profile* profile, bool browser_launched) = 0;

  virtual base::WeakPtr<UserSessionManagerDelegate> AsWeakPtr() = 0;

 protected:
  virtual ~UserSessionManagerDelegate();
};

class UserSessionStateObserver {
 public:
  // Called when UserManager finishes restoring user sessions after crash.
  virtual void PendingUserSessionsRestoreFinished();

 protected:
  virtual ~UserSessionStateObserver();
};

class UserAuthenticatorObserver : public base::CheckedObserver {
 public:
  // Called when authentication is started.
  virtual void OnAuthAttemptStarted() {}
};

// UserSessionManager is responsible for starting user session which includes:
// * load and initialize Profile (including custom Profile preferences),
// * mark user as logged in and notify observers,
// * initialize OAuth2 authentication session,
// * initialize and launch user session based on the user type.
// Also supports restoring active user sessions after browser crash:
// load profile, restore OAuth authentication session etc.
class UserSessionManager
    : public OAuth2LoginManager::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public UserSessionManagerDelegate,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Context of StartSession calls.
  enum class StartSessionType {
    // No StartSession call happened yet.
    kNone,

    // Starting primary user session, through login UI.
    kPrimary,

    // Starting secondary user session, through multi-profiles login UI.
    kSecondary,

    // Starting primary user session after browser crash.
    kPrimaryAfterCrash,

    // Starting secondary user session after browser crash.
    kSecondaryAfterCrash,
  };

  // Types of command-line switches for a user session. The command-line
  // switches of all types are combined.
  enum class CommandLineSwitchesType {
    // Switches for controlling session initialization, such as if the profile
    // requires enterprise policy.
    kSessionControl,
    // Switches derived from user policy and kiosk app control switches.
    // TODO(pmarko): Split this into multiple categories, such as kPolicy,
    // kKioskControl. Consider also adding sentinels automatically and
    // pre-filling these switches from the command-line if the chrome has been
    // started with the --login-user flag (https://crbug.com/832857).
    kPolicyAndKioskControl
  };

  // To keep track of which systems need the login password to be stored in the
  // kernel keyring.
  enum class PasswordConsumingService {
    // Shill needs the login password if ${PASSWORD} is specified somewhere in
    // the OpenNetworkConfiguration policy.
    kNetwork = 0,

    // The Kerberos service needs the login password if ${PASSWORD} is specified
    // somewhere in the KerberosAccounts policy.
    kKerberos = 1,

    // Should be last. All enum values must be consecutive starting from 0.
    kCount = 2,
  };

  // Returns UserSessionManager instance.
  static UserSessionManager* GetInstance();

  UserSessionManager(const UserSessionManager&) = delete;
  UserSessionManager& operator=(const UserSessionManager&) = delete;

  // Registers session related preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Applies user policies to `flags`. This could mean removing flags that have
  // been added by the flag handling logic or appending additional flags due to
  // enterprise policy.
  static void ApplyUserPolicyToFlags(PrefService* user_profile_prefs,
                                     std::set<std::string>* flags);

  // Invoked after the tmpfs is successfully mounted.
  // Asks session_manager to restart Chrome in Guest session mode.
  // `start_url` is an optional URL to be opened in Guest session browser.
  void CompleteGuestSessionLogin(const GURL& start_url);

  // Creates and returns the authenticator to use.
  // Single Authenticator instance is used for entire login process,
  // even for multiple retries. Authenticator instance holds reference to
  // login profile and is later used during fetching of OAuth tokens.
  scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer);

  // Start user session given `user_context`.
  // OnProfilePrepared() will be called on `delegate` once Profile is ready.
  void StartSession(const UserContext& user_context,
                    StartSessionType start_session_type,
                    bool has_auth_cookies,
                    bool has_active_session,
                    base::WeakPtr<UserSessionManagerDelegate> delegate);

  // Restores authentication session after crash.
  void RestoreAuthenticationSession(Profile* profile);

  // Usually is called when Chrome is restarted after a crash and there's an
  // active session. First user (one that is passed with --login-user) Chrome
  // session has been already restored at this point. This method asks session
  // manager for all active user sessions, marks them as logged in
  // and notifies observers.
  void RestoreActiveSessions();

  // Returns true iff browser has been restarted after crash and
  // UserSessionManager finished restoring user sessions.
  bool UserSessionsRestored() const;

  // Returns true iff browser has been restarted after crash and
  // user sessions restoration is in progress.
  bool UserSessionsRestoreInProgress() const;

  // Called when user profile is loaded. Send the notification before creating
  // the browser so additional objects that need the profile (e.g. the launcher)
  // can be created first.
  void OnUserProfileLoaded(Profile* profile, const user_manager::User* user);

  // Start the Tether service if it is ready.
  void StartTetherServiceIfPossible(Profile* profile);

  // Show various notifications if applicable.
  void ShowNotificationsIfNeeded(Profile* profile);

  // Perform actions that were deferred from OOBE or onboarding flow once the
  // browser is launched if applicable.
  void PerformPostBrowserLaunchOOBEActions(Profile* profile);

  // Invoked when the user is logging in for the first time, or is logging in to
  // an ephemeral session type, such as guest or a public session.
  void SetFirstLoginPrefs(Profile* profile,
                          const std::string& public_session_locale,
                          const std::string& public_session_input_method);

  // Thin wrapper around StartupBrowserCreator::LaunchBrowser().  Meant to be
  // used in a Task posted to the UI thread.  Once the browser is launched the
  // login host is deleted.
  void DoBrowserLaunch(Profile* profile);

  // Changes browser locale (selects best suitable locale from different
  // user settings). Returns true if callback will be called.
  bool RespectLocalePreference(
      Profile* profile,
      const user_manager::User* user,
      locale_util::SwitchLanguageCallback callback) const;

  // Switch to the locale that `profile` wishes to use and invoke `callback`.
  void RespectLocalePreferenceWrapper(Profile* profile,
                                      base::OnceClosure callback);

  void LaunchBrowser(Profile* profile);

  // Restarts Chrome if needed. This happens when user session has custom
  // flags/switches enabled. Another case when owner has setup custom flags,
  // they are applied on login screen as well but not to user session.
  // `early_restart` is true if this restart attempt happens before user profile
  // is fully initialized.
  // Might not return if restart is possible right now.
  // Returns true if restart was scheduled.
  // Returns false if no restart is needed.
  bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                           bool early_restart);

  void AddSessionStateObserver(ash::UserSessionStateObserver* observer);
  void RemoveSessionStateObserver(ash::UserSessionStateObserver* observer);

  void AddUserAuthenticatorObserver(UserAuthenticatorObserver* observer);
  void RemoveUserAuthenticatorObserver(UserAuthenticatorObserver* observer);

  void ActiveUserChanged(user_manager::User* active_user) override;

  // Returns default IME state for user session.
  scoped_refptr<input_method::InputMethodManager::State> GetDefaultIMEState(
      Profile* profile);

  // Check to see if given profile should show EndOfLife Notification
  // and show the message accordingly.
  void CheckEolInfo(Profile* profile);

  // Removes a profile from the per-user input methods states map.
  void RemoveProfileForTesting(Profile* profile);

  const UserContext& user_context() const { return user_context_; }
  bool has_auth_cookies() const { return has_auth_cookies_; }

  const base::Time& ui_shown_time() const { return ui_shown_time_; }

  void Shutdown();

  // Sets the command-line switches to be set by session manager for a user
  // session associated with `account_id` when chrome restarts. Overwrites
  // switches for `switches_type` with `switches`. The resulting command-line
  // switches will be the command-line switches for all types combined. Note:
  // `account_id` is currently ignored, because session manager ignores the
  // passed account id. For each type, only the last-set switches will be
  // honored.
  // TODO(pmarko): Introduce a CHECK making sure that `account_id` is the
  // primary user (https://crbug.com/832857).
  void SetSwitchesForUser(const AccountId& account_id,
                          CommandLineSwitchesType switches_type,
                          const std::vector<std::string>& switches);

  // This should only be called when the primary user session is being
  // initialized. Calls outside of the primary user session initialization will
  // be ignored.
  // Notify whether `service` wants session manager to save the user's login
  // password. If `save_password` is true, the login password is sent over D-Bus
  // to the session manager to save in a keyring. Once this method has been
  // called from all services defined in `PasswordConsumingService`, or if
  // `save_password` is true, the method clears the user password from the
  // UserContext before exiting.
  // Should be called for each `service` in `PasswordConsumingService` as soon
  // as the service knows whether it needs the login password. Must be called
  // before user_context_.ClearSecrets() (see .cc), where Chrome 'forgets' the
  // password.
  void VoteForSavingLoginPassword(PasswordConsumingService service,
                                  bool save_password);

  UserContext* mutable_user_context_for_testing() { return &user_context_; }
  void set_start_session_type_for_testing(StartSessionType start_session_type) {
    start_session_type_ = start_session_type;
  }

  bool token_handle_backfill_tried_for_testing() {
    return token_handle_backfill_tried_for_testing_;
  }

  // Shows U2F notification if necessary.
  void MaybeShowU2FNotification();

  // Shows Help App release notes notification, if a notification for the help
  // app has not yet been shown in the current milestone.
  void MaybeShowHelpAppReleaseNotesNotification(Profile* profile);

  using EolNotificationHandlerFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<EolNotification>(
          Profile* profile)>;
  void SetEolNotificationHandlerFactoryForTesting(
      const EolNotificationHandlerFactoryCallback& eol_notification_factory);

  base::WeakPtr<UserSessionManager> GetUserSessionManagerAsWeakPtr();

 protected:
  // Protected for testability reasons.
  UserSessionManager();
  ~UserSessionManager() override;

 private:
  // Observes the Device Account's LST and informs UserSessionManager about it.
  class DeviceAccountGaiaTokenObserver;
  friend class test::UserSessionManagerTestApi;
  friend struct base::DefaultSingletonTraits<UserSessionManager>;

  using SigninSessionRestoreStateSet = std::set<AccountId>;

  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // OAuth2LoginManager::Observer overrides:
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // UserSessionManagerDelegate overrides:
  // Used when restoring user sessions after crash.
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;
  base::WeakPtr<UserSessionManagerDelegate> AsWeakPtr() override;

  void ChildAccountStatusReceivedCallback(Profile* profile);

  void StopChildStatusObserving(Profile* profile);

  void CreateUserSession(const UserContext& user_context,
                         bool has_auth_cookies);
  void PreStartSession(StartSessionType start_session_type);

  // Store any useful UserContext data early on when profile has not been
  // created yet and user services were not yet initialized. Can store
  // information in Local State like GAIA ID.
  void StoreUserContextDataBeforeProfileIsCreated();

  // Initializes `DemoSession` if starting user session for demo mode.
  // Runs `callback` when demo session initialization finishes, i.e. when the
  // offline demo session resources are loaded. In addition, disables browser
  // launch if demo session is started.
  void InitDemoSessionIfNeeded(base::OnceClosure callback);

  // Updates ARC file system compatibility pref, and then calls
  // PrepareProfile().
  void UpdateArcFileSystemCompatibilityAndPrepareProfile();

  void InitializeAccountManager();

  void StartCrosSession();
  void PrepareProfile(const base::FilePath& profile_path);

  // Callback for Profile::CREATE_STATUS_CREATED profile state.
  // Initializes basic preferences for newly created profile. Any other
  // early profile initialization that needs to happen before
  // ProfileManager::DoFinalInit() gets called is done here.
  void InitProfilePreferences(Profile* profile,
                              const UserContext& user_context);

  // Initializes `user_context` and `known_user` with a device id. Does not
  // overwrite the device id in `known_user` if it already exists.
  void InitializeDeviceId(bool is_ephemeral_user,
                          UserContext& user_context,
                          user_manager::KnownUser& known_user);

  // Callback for Profile::CREATE_STATUS_INITIALIZED profile state.
  // Profile is created, extensions and promo resources are initialized.
  void UserProfileInitialized(Profile* profile, const AccountId& account_id);

  // Callback to resume profile creation after transferring auth data from
  // the authentication profile.
  void CompleteProfileCreateAfterAuthTransfer(Profile* profile);

  // Finalized profile preparation.
  void FinalizePrepareProfile(Profile* profile);

  // Launch browser or proceed to alternative login flow. Should be called after
  // profile is ready.
  void InitializeBrowser(Profile* profile);

  // Launches the Help App depending on flags / prefs / user. This should only
  // be used for the first run experience, i.e. after the user completed the
  // OOBE setup.
  void MaybeLaunchHelpAppForFirstRun(Profile* profile) const;

  // Start user onboarding if the user is new.
  bool MaybeStartNewUserOnboarding(Profile* profile);

  // Perform session initialization and either move to additional login flows
  // such as TOS (public sessions), priority pref sync UI (new users) or
  // launch browser.
  // Returns true if browser has been launched or false otherwise.
  bool InitializeUserSession(Profile* profile);

  // Processes App mode command-line switches.
  void ProcessAppModeSwitches();

  // Restores GAIA auth cookies for the created user profile from OAuth2 token.
  void RestoreAuthSessionImpl(Profile* profile, bool restore_from_auth_cookies);

  // Callback to process RetrieveActiveSessions() request results.
  void OnRestoreActiveSessions(
      std::optional<SessionManagerClient::ActiveSessionsMap> sessions);

  // Called by OnRestoreActiveSessions() when there're user sessions in
  // `pending_user_sessions_` that has to be restored one by one.
  // Also called after first user session from that list is restored and so on.
  // Process continues till `pending_user_sessions_` map is not empty.
  void RestorePendingUserSessions();

  // Notifies observers that user pending sessions restore has finished.
  void NotifyPendingUserSessionsRestoreFinished();

  // Callback invoked when child policy is ready and the session for child user
  // can be started.
  void OnChildPolicyReady(
      Profile* profile,
      ChildPolicyObserver::InitialPolicyRefreshResult result);

  // Internal implementation of DoBrowserLaunch. Initially should be called with
  // `locale_pref_checked` set to false which will result in postponing browser
  // launch till user locale is applied if needed. After locale check has
  // completed this method is called with `locale_pref_checked` set to true.
  void DoBrowserLaunchInternal(Profile* profile, bool locale_pref_checked);

  static void RunCallbackOnLocaleLoaded(
      base::OnceClosure callback,
      InputEventsBlocker* input_events_blocker,
      const locale_util::LanguageSwitchResult& result);

  // Callback invoked when `token_handle_util_` has finished.
  void OnTokenHandleObtained(const AccountId& account_id, bool success);

  // Returns `true` if token handles should be used on this device.
  bool TokenHandlesEnabled();

  void CreateTokenUtilIfMissing();

  // Update token handle if the existing token handle is missing/invalid.
  void UpdateTokenHandleIfRequired(Profile* const profile,
                                   const AccountId& account_id);

  // Force update token handle.
  void UpdateTokenHandle(Profile* const profile, const AccountId& account_id);

  // Test API methods.
  void InjectAuthenticatorBuilder(
      std::unique_ptr<AuthenticatorBuilder> builder);

  // Controls whether browser instance should be launched after sign in
  // (used in tests).
  void set_should_launch_browser_in_tests(bool should_launch_browser) {
    should_launch_browser_ = should_launch_browser;
  }

  // Controls whether token handle fetching is enabled (used in tests).
  void SetShouldObtainHandleInTests(bool should_obtain_handles);

  // Sets the function which is used to request a chrome restart.
  void SetAttemptRestartClosureInTests(
      const base::RepeatingClosure& attempt_restart_closure);

  bool IsFullRestoreEnabled(Profile* profile);

  void OnUserEligibleForOnboardingSurvey(Profile* profile);

  // Triggers loading of the shill profile for |account_id|. This should only be
  // called for the primary user session.
  void LoadShillProfile(const AccountId& account_id);

  // Get a reference the help app notification controller, creating it if it
  // doesn't exist.
  HelpAppNotificationController* GetHelpAppNotificationController(
      Profile* profile);

  base::WeakPtr<UserSessionManagerDelegate> delegate_;

  // Used to listen to network changes.
  raw_ptr<network::NetworkConnectionTracker, LeakedDanglingUntriaged>
      network_connection_tracker_;

  // Authentication/user context.
  UserContext user_context_;
  scoped_refptr<Authenticator> authenticator_;
  StartSessionType start_session_type_ = StartSessionType::kNone;

  std::unique_ptr<AuthenticatorBuilder> injected_authenticator_builder_;

  // True if the authentication context's cookie jar contains authentication
  // cookies from the authentication extension login flow.
  bool has_auth_cookies_;

  // Active user session restoration related members.

  // True if user sessions has been restored after crash.
  // On a normal boot then login into user sessions this will be false.
  bool user_sessions_restored_;

  // True if user sessions restoration after crash is in progress.
  bool user_sessions_restore_in_progress_;

  // User sessions that have to be restored after browser crash.
  // [user_id] > [user_id_hash]
  using PendingUserSessions = std::map<AccountId, std::string>;

  PendingUserSessions pending_user_sessions_;

  base::ObserverList<ash::UserSessionStateObserver>::Unchecked
      session_state_observer_list_;

  base::ObserverList<UserAuthenticatorObserver> authenticator_observer_list_;

  // Set of user_id for those users that we should restore authentication
  // session when notified about online state change.
  SigninSessionRestoreStateSet pending_signin_restore_sessions_;

  // Per-user-session Input Methods states.
  std::map<Profile*,
           scoped_refptr<input_method::InputMethodManager::State>,
           ProfileCompare>
      default_ime_states_;

  // Per-user-session EndofLife Notification
  std::map<Profile*, std::unique_ptr<EolNotification>, ProfileCompare>
      eol_notification_handler_;

  // Keeps track of which password-requiring-service has already told us whether
  // they need the login password or not.
  std::bitset<static_cast<size_t>(PasswordConsumingService::kCount)>
      password_service_voted_;
  // Whether the login password was saved in the kernel keyring.
  bool password_was_saved_ = false;

  // Maps command-line switch types to the currently set command-line
  // switches for that type. Note: This is not per Profile/AccountId,
  // because session manager currently doesn't support setting command-line
  // switches per AccountId.
  base::flat_map<CommandLineSwitchesType, std::vector<std::string>>
      command_line_switches_;

  // Whether should fetch token handles, tests may override this value.
  bool should_obtain_handles_;

  std::unique_ptr<TokenHandleUtil> token_handle_util_;
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher_;
  std::map<Profile*, std::unique_ptr<DeviceAccountGaiaTokenObserver>>
      token_observers_;

  // Whether should launch browser, tests may override this value.
  bool should_launch_browser_;

  // Child account status is necessary for InitializeStartUrls call.
  bool waiting_for_child_account_status_;

  // If set then contains the time when UI is shown.
  base::Time ui_shown_time_;

  scoped_refptr<HatsNotificationController> hats_notification_controller_;

  // Mapped to `chrome::AttemptRestart`, except in tests.
  base::RepeatingClosure attempt_restart_closure_;

  base::flat_set<raw_ptr<Profile, CtnExperimental>>
      user_profile_initialized_called_;

  std::unique_ptr<arc::AlwaysOnVpnManager> always_on_vpn_manager_;

  std::unique_ptr<XdrManager> xdr_manager_;

  std::unique_ptr<ChildPolicyObserver> child_policy_observer_;

  std::unique_ptr<U2FNotification> u2f_notification_;

  std::unique_ptr<HelpAppNotificationController>
      help_app_notification_controller_;

  bool token_handle_backfill_tried_for_testing_ = false;

  std::unique_ptr<OnboardingUserActivityCounter>
      onboarding_user_activity_counter_;

  // Callback that allows tests to inject a test EolNotification implementation.
  EolNotificationHandlerFactoryCallback eol_notification_handler_test_factory_;

  // Whether `metrics::BeginFirstWebContentsProfiling()` has been called. Should
  // only be called once per program lifetime.
  bool has_recorded_first_web_contents_metrics_ = false;

  base::WeakPtrFactory<UserSessionManager> weak_factory_{this};
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::UserSessionManager,
                               ash::UserAuthenticatorObserver> {
  static void AddObserver(ash::UserSessionManager* source,
                          ash::UserAuthenticatorObserver* observer) {
    source->AddUserAuthenticatorObserver(observer);
  }
  static void RemoveObserver(ash::UserSessionManager* source,
                             ash::UserAuthenticatorObserver* observer) {
    source->RemoveUserAuthenticatorObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_MANAGER_H_
