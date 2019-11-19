// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_

#include <bitset>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/child_accounts/child_policy_observer.h"
#include "chrome/browser/chromeos/eol_notification.h"
#include "chrome/browser/chromeos/hats/hats_notification_controller.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/token_handle_util.h"
#include "chrome/browser/chromeos/release_notes/release_notes_notification.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/u2f_notification.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/arc/net/always_on_vpn_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

class AccountId;
class GURL;
class PrefRegistrySimple;
class PrefService;
class Profile;
class TokenHandleFetcher;

namespace base {
class CommandLine;
}

namespace user_manager {
class User;
}  // namespace user_manager

namespace chromeos {

namespace test {
class UserSessionManagerTestApi;
}  // namespace test

class EasyUnlockKeyManager;
class InputEventsBlocker;
class LoginDisplayHost;
class StubAuthenticatorBuilder;

class UserSessionManagerDelegate {
 public:
  // Called after profile is loaded and prepared for the session.
  // |browser_launched| will be true is browser has been launched, otherwise
  // it will return false and client is responsible on launching browser.
  virtual void OnProfilePrepared(Profile* profile, bool browser_launched) = 0;

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
      public base::SupportsWeakPtr<UserSessionManager>,
      public UserSessionManagerDelegate,
      public user_manager::UserManager::UserSessionStateObserver,
      public user_manager::UserManager::Observer {
 public:
  // Context of StartSession calls.
  typedef enum {
    // Starting primary user session, through login UI.
    PRIMARY_USER_SESSION,

    // Starting secondary user session, through multi-profiles login UI.
    SECONDARY_USER_SESSION,

    // Starting primary user session after browser crash.
    PRIMARY_USER_SESSION_AFTER_CRASH,

    // Starting secondary user session after browser crash.
    SECONDARY_USER_SESSION_AFTER_CRASH,
  } StartSessionType;

  // Types of command-line switches for a user session. The command-line
  // switches of all types are combined.
  enum class CommandLineSwitchesType {
    // Switches for controlling session initialization, such as if the profile
    // requires enterprise policy.
    kSessionControl,
    // Switches derived from user policy, from user-set flags and kiosk app
    // control switches.
    // TODO(pmarko): Split this into multiple categories, such as kPolicy,
    // kFlags, kKioskControl. Consider also adding sentinels automatically and
    // pre-filling these switches from the command-line if the chrome has been
    // started with the --login-user flag (https://crbug.com/832857).
    kPolicyAndFlagsAndKioskControl
  };

  // Parameters to use when initializing the RLZ library.  These fields need
  // to be retrieved from a blocking task and this structure is used to pass
  // the data.
  struct RlzInitParams {
    // Set to true if RLZ is disabled.
    bool disabled;

    // The elapsed time since the device went through the OOBE.  This can
    // be a very long time.
    base::TimeDelta time_since_oobe_completion;
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

  // Called when user is logged in to override base::DIR_HOME path.
  static void OverrideHomedir();

  // Registers session related preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Applies user policies to |user_flags| .
  // This could mean removing command-line switchis that have been added by the
  // flag handling logic or appending additional switches due to policy.
  static void ApplyUserPolicyToSwitches(PrefService* user_profile_prefs,
                                        base::CommandLine* user_flags);

  // Invoked after the tmpfs is successfully mounted.
  // Asks session_manager to restart Chrome in Guest session mode.
  // |start_url| is an optional URL to be opened in Guest session browser.
  void CompleteGuestSessionLogin(const GURL& start_url);

  // Creates and returns the authenticator to use.
  // Single Authenticator instance is used for entire login process,
  // even for multiple retries. Authenticator instance holds reference to
  // login profile and is later used during fetching of OAuth tokens.
  scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer);

  // Start user session given |user_context|.
  // OnProfilePrepared() will be called on |delegate| once Profile is ready.
  void StartSession(const UserContext& user_context,
                    StartSessionType start_session_type,
                    bool has_auth_cookies,
                    bool has_active_session,
                    UserSessionManagerDelegate* delegate);

  // Invalidates |delegate|, which was passed to StartSession method call.
  void DelegateDeleted(UserSessionManagerDelegate* delegate);

  // Perform additional actions once system wide notification
  // "UserLoggedIn" has been sent.
  void PerformPostUserLoggedInActions();

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

  // Initialize RLZ.
  void InitRlz(Profile* profile);

  // Get the NSS cert database for the user represented with |profile|
  // and start certificate loader with it.
  void InitializeCerts(Profile* profile);

  // Starts loading CRL set.
  void InitializeCRLSetFetcher(const user_manager::User* user);

  // Initializes Certificate Transparency-related components.
  void InitializeCertificateTransparencyComponents(
      const user_manager::User* user);

  // Invoked when the user is logging in for the first time, or is logging in to
  // an ephemeral session type, such as guest or a public session.
  void SetFirstLoginPrefs(Profile* profile,
                          const std::string& public_session_locale,
                          const std::string& public_session_input_method);

  // Gets/sets Chrome OAuth client id and secret for kiosk app mode. The default
  // values can be overridden with kiosk auth file.
  bool GetAppModeChromeClientOAuthInfo(std::string* chrome_client_id,
                                       std::string* chrome_client_secret);
  void SetAppModeChromeClientOAuthInfo(const std::string& chrome_client_id,
                                       const std::string& chrome_client_secret);

  // Thin wrapper around StartupBrowserCreator::LaunchBrowser().  Meant to be
  // used in a Task posted to the UI thread.  Once the browser is launched the
  // login host is deleted.
  void DoBrowserLaunch(Profile* profile, LoginDisplayHost* login_host);

  // Changes browser locale (selects best suitable locale from different
  // user settings). Returns true if callback will be called.
  bool RespectLocalePreference(
      Profile* profile,
      const user_manager::User* user,
      const locale_util::SwitchLanguageCallback& callback) const;

  // Switch to the locale that |profile| wishes to use and invoke |callback|.
  void RespectLocalePreferenceWrapper(Profile* profile,
                                      const base::Closure& callback);

  // Restarts Chrome if needed. This happens when user session has custom
  // flags/switches enabled. Another case when owner has setup custom flags,
  // they are applied on login screen as well but not to user session.
  // |early_restart| is true if this restart attempt happens before user profile
  // is fully initialized.
  // Might not return if restart is possible right now.
  // Returns true if restart was scheduled.
  // Returns false if no restart is needed.
  bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                           bool early_restart);

  // Returns true if Easy unlock keys needs to be updated.
  bool NeedsToUpdateEasyUnlockKeys() const;

  void AddSessionStateObserver(chromeos::UserSessionStateObserver* observer);
  void RemoveSessionStateObserver(chromeos::UserSessionStateObserver* observer);

  void ActiveUserChanged(user_manager::User* active_user) override;

  // This method will be called when user have obtained oauth2 tokens.
  void OnOAuth2TokensFetched(UserContext context);

  // Returns default IME state for user session.
  scoped_refptr<input_method::InputMethodManager::State> GetDefaultIMEState(
      Profile* profile);

  // Check to see if given profile should show EndOfLife Notification
  // and show the message accordingly.
  void CheckEolInfo(Profile* profile);

  // Starts migrating accounts to Chrome OS Account Manager.
  void StartAccountManagerMigration(Profile* profile);

  // Note this could return NULL if not enabled.
  EasyUnlockKeyManager* GetEasyUnlockKeyManager();

  // Update Easy unlock cryptohome keys for given user context.
  void UpdateEasyUnlockKeys(const UserContext& user_context);

  // Removes a profile from the per-user input methods states map.
  void RemoveProfileForTesting(Profile* profile);

  const UserContext& user_context() const { return user_context_; }
  bool has_auth_cookies() const { return has_auth_cookies_; }

  const base::Time& ui_shown_time() const { return ui_shown_time_; }

  void WaitForEasyUnlockKeyOpsFinished(base::OnceClosure callback);

  void Shutdown();

  // Sets the command-line switches to be set by session manager for a user
  // session associated with |account_id| when chrome restarts. Overwrites
  // switches for |switches_type| with |switches|. The resulting command-line
  // switches will be the command-line switches for all types combined. Note:
  // |account_id| is currently ignored, because session manager ignores the
  // passed account id. For each type, only the last-set switches will be
  // honored.
  // TODO(pmarko): Introduce a CHECK making sure that |account_id| is the
  // primary user (https://crbug.com/832857).
  void SetSwitchesForUser(const AccountId& account_id,
                          CommandLineSwitchesType switches_type,
                          const std::vector<std::string>& switches);

  // Notify whether |service| wants session manager to save the user's login
  // password. If |save_password| is true, the login password is sent over D-Bus
  // to the session manager to save in a keyring. Once this method has been
  // called from all services defined in |PasswordConsumingService|, or if
  // |save_password| is true, the method clears the user password from the
  // UserContext before exiting.
  // Should be called for each |service| in |PasswordConsumingService| as soon
  // as the service knows whether it needs the login password. Must be called
  // before user_context_.ClearSecrets() (see .cc), where Chrome 'forgets' the
  // password.
  void VoteForSavingLoginPassword(PasswordConsumingService service,
                                  bool save_password);

  UserContext* mutable_user_context_for_testing() { return &user_context_; }

  // Shows U2F notification if necessary.
  void MaybeShowU2FNotification();

  // Shows Release Notes notification if necessary.
  void MaybeShowReleaseNotesNotification(Profile* profile);

 protected:
  // Protected for testability reasons.
  UserSessionManager();
  ~UserSessionManager() override;

 private:
  friend class test::UserSessionManagerTestApi;
  friend struct base::DefaultSingletonTraits<UserSessionManager>;

  typedef std::set<std::string> SigninSessionRestoreStateSet;

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

  // user_manager::UserManager::Observer overrides:
  void OnUsersSignInConstraintsChanged() override;

  void ChildAccountStatusReceivedCallback(Profile* profile);

  void StopChildStatusObserving(Profile* profile);

  void CreateUserSession(const UserContext& user_context,
                         bool has_auth_cookies);
  void PreStartSession();

  // Store any useful UserContext data early on when profile has not been
  // created yet and user services were not yet initialized. Can store
  // information in Local State like GAIA ID.
  void StoreUserContextDataBeforeProfileIsCreated();

  // Initializes |chromeos::DemoSession| if starting user session for demo mode.
  // Runs |callback| when demo session initialization finishes, i.e. when the
  // offline demo session resources are loaded. In addition, disables browser
  // launch if demo session is started.
  void InitDemoSessionIfNeeded(base::OnceClosure callback);

  // Updates ARC file system compatibility pref, and then calls
  // PrepareProfile().
  void UpdateArcFileSystemCompatibilityAndPrepareProfile();

  void InitializeAccountManager();

  void StartCrosSession();
  void PrepareProfile(const base::FilePath& profile_path);

  // Callback for asynchronous profile creation.
  void OnProfileCreated(const UserContext& user_context,
                        bool is_incognito_profile,
                        Profile* profile,
                        Profile::CreateStatus status);

  // Callback for Profile::CREATE_STATUS_CREATED profile state.
  // Initializes basic preferences for newly created profile. Any other
  // early profile initialization that needs to happen before
  // ProfileManager::DoFinalInit() gets called is done here.
  void InitProfilePreferences(Profile* profile,
                              const UserContext& user_context);

  // Callback for Profile::CREATE_STATUS_INITIALIZED profile state.
  // Profile is created, extensions and promo resources are initialized.
  void UserProfileInitialized(Profile* profile,
                              bool is_incognito_profile,
                              const AccountId& account_id);

  // Callback to resume profile creation after transferring auth data from
  // the authentication profile.
  void CompleteProfileCreateAfterAuthTransfer(Profile* profile);

  // Asynchronously prepares TPM devices and calls FinalizePrepareProfile on UI
  // thread.
  void PrepareTpmDeviceAndFinalizeProfile(Profile* profile);

  // Called on UI thread once Cryptohome operation completes.
  void OnCryptohomeOperationCompleted(Profile* profile, bool result);

  // Finalized profile preparation.
  void FinalizePrepareProfile(Profile* profile);

  // Launch browser or proceed to alternative login flow. Should be called after
  // profile is ready.
  void InitializeBrowser(Profile* profile);

  // Initialize child user profile services that depend on the policy.
  void InitializeChildUserServices(Profile* profile);

  // Starts out-of-box flow with the specified screen.
  void ActivateWizard(OobeScreenId screen);

  // Adds first-time login URLs.
  void InitializeStartUrls() const;

  // Perform session initialization and either move to additional login flows
  // such as TOS (public sessions), priority pref sync UI (new users) or
  // launch browser.
  // Returns true if browser has been launched or false otherwise.
  bool InitializeUserSession(Profile* profile);

  // Initializes member variables needed for session restore process via
  // OAuthLoginManager.
  void InitSessionRestoreStrategy();

  // Restores GAIA auth cookies for the created user profile from OAuth2 token.
  void RestoreAuthSessionImpl(Profile* profile, bool restore_from_auth_cookies);

  // Initializes RLZ. If |disabled| is true, RLZ pings are disabled.
  void InitRlzImpl(Profile* profile, const RlzInitParams& params);

  // If |user| is not a kiosk app, sets session type as seen by extensions
  // feature system according to |user|'s type.
  // The value should eventually be set for kiosk users, too - that's done as
  // part of special, kiosk user session bring-up.
  // NOTE: This has to be called before profile is initialized - so it is set up
  // when extension are loaded during profile initialization.
  void InitNonKioskExtensionFeaturesSessionType(const user_manager::User* user);

  // Callback to process RetrieveActiveSessions() request results.
  void OnRestoreActiveSessions(
      base::Optional<SessionManagerClient::ActiveSessionsMap> sessions);

  // Called by OnRestoreActiveSessions() when there're user sessions in
  // |pending_user_sessions_| that has to be restored one by one.
  // Also called after first user session from that list is restored and so on.
  // Process continues till |pending_user_sessions_| map is not empty.
  void RestorePendingUserSessions();

  // Notifies observers that user pending sessions restore has finished.
  void NotifyPendingUserSessionsRestoreFinished();

  // Callback invoked when Easy unlock key operations are finished.
  void OnEasyUnlockKeyOpsFinished(const std::string& user_id, bool success);

  // Callback invoked when child policy is ready and the session for child user
  // can be started.
  void OnChildPolicyReady(
      Profile* profile,
      ChildPolicyObserver::InitialPolicyRefreshResult result);

  // Internal implementation of DoBrowserLaunch. Initially should be called with
  // |locale_pref_checked| set to false which will result in postponing browser
  // launch till user locale is applied if needed. After locale check has
  // completed this method is called with |locale_pref_checked| set to true.
  void DoBrowserLaunchInternal(Profile* profile,
                               LoginDisplayHost* login_host,
                               bool locale_pref_checked);

  static void RunCallbackOnLocaleLoaded(
      const base::Closure& callback,
      InputEventsBlocker* input_events_blocker,
      const locale_util::LanguageSwitchResult& result);

  // Callback invoked when |token_handle_util_| has finished.
  void OnTokenHandleObtained(const AccountId& account_id, bool success);

  // Returns |true| if token handles should be used on this device.
  bool TokenHandlesEnabled();

  void CreateTokenUtilIfMissing();

  // Test API methods.
  void InjectAuthenticatorBuilder(
      std::unique_ptr<StubAuthenticatorBuilder> builer);

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

  // The user pods display type for histogram.
  enum UserPodsDisplay {
    // User pods enabling or disabling is possible either via local settings or
    // via domain policy. The former method only applies to regular devices,
    // whereas the latter is for enterprise-managed devices. Therefore, we have
    // four possible combiations.
    USER_PODS_DISPLAY_ENABLED_REGULAR = 0,
    USER_PODS_DISPLAY_ENABLED_MANAGED = 1,
    USER_PODS_DISPLAY_DISABLED_REGULAR = 2,
    USER_PODS_DISPLAY_DISABLED_MANAGED = 3,
    // Maximum histogram value.
    NUM_USER_PODS_DISPLAY = 4
  };

  // Sends metrics for user pods display when existing user has logged in.
  void SendUserPodsMetrics();

  void NotifyEasyUnlockKeyOpsFinished();

  UserSessionManagerDelegate* delegate_;

  // Used to listen to network changes.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Authentication/user context.
  UserContext user_context_;
  scoped_refptr<Authenticator> authenticator_;
  StartSessionType start_session_type_;

  std::unique_ptr<StubAuthenticatorBuilder> injected_authenticator_builder_;

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

  base::ObserverList<chromeos::UserSessionStateObserver>::Unchecked
      session_state_observer_list_;

  // OAuth2 session related members.

  // Sesion restore strategy.
  OAuth2LoginManager::SessionRestoreStrategy session_restore_strategy_;

  // Set of user_id for those users that we should restore authentication
  // session when notified about online state change.
  SigninSessionRestoreStateSet pending_signin_restore_sessions_;

  // Kiosk mode related members.
  // Chrome oauth client id and secret - override values for kiosk mode.
  std::string chrome_client_id_;
  std::string chrome_client_secret_;

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

  // Manages Easy unlock cryptohome keys.
  std::unique_ptr<EasyUnlockKeyManager> easy_unlock_key_manager_;
  bool running_easy_unlock_key_ops_;

  // Whether should fetch token handles, tests may override this value.
  bool should_obtain_handles_;

  std::unique_ptr<TokenHandleUtil> token_handle_util_;
  std::unique_ptr<TokenHandleFetcher> token_handle_fetcher_;

  // Whether should launch browser, tests may override this value.
  bool should_launch_browser_;

  // Child account status is necessary for InitializeStartUrls call.
  bool waiting_for_child_account_status_;

  // If set then contains the time when UI is shown.
  base::Time ui_shown_time_;

  scoped_refptr<HatsNotificationController> hats_notification_controller_;

  bool easy_unlock_key_ops_finished_ = true;

  std::vector<base::OnceClosure> easy_unlock_key_ops_finished_callbacks_;

  // Mapped to |chrome::AttemptRestart|, except in tests.
  base::RepeatingClosure attempt_restart_closure_;

  std::unique_ptr<arc::AlwaysOnVpnManager> always_on_vpn_manager_;

  std::unique_ptr<ChildPolicyObserver> child_policy_observer_;

  std::unique_ptr<U2FNotification> u2f_notification_;

  std::unique_ptr<ReleaseNotesNotification> release_notes_notification_;

  base::WeakPtrFactory<UserSessionManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserSessionManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_
