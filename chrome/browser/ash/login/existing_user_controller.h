// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "url/gurl.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace ash {
class CrosSettings;
class KioskAppId;
class OAuth2TokenInitializer;
enum class SigninError;

namespace login {
class NetworkStateHelper;
}

namespace quick_unlock {
class PinSaltStorage;
}

// ExistingUserController is used to handle login when someone has already
// logged into the machine. ExistingUserController is created and owned by
// LoginDisplayHost.
class ExistingUserController : public HttpAuthDialog::Observer,
                               public LoginPerformer::Delegate,
                               public UserSessionManagerDelegate,
                               public user_manager::UserManager::Observer,
                               public ui::UserActivityObserver {
 public:
  // Returns the current existing user controller fetched from the current
  // LoginDisplayHost instance.
  static ExistingUserController* current_controller();

  // All UI initialization is deferred till Init() call.
  ExistingUserController();

  ExistingUserController(const ExistingUserController&) = delete;
  ExistingUserController& operator=(const ExistingUserController&) = delete;

  ~ExistingUserController() override;

  // Creates and shows login UI for known users.
  void Init(const user_manager::UserList& users);

  // Start the auto-login timer.
  void StartAutoLoginTimer();

  // Stop the auto-login timer when a login attempt begins.
  void StopAutoLoginTimer();

  // Cancels current password changed flow.
  void CancelPasswordChangedFlow();

  // Resumes login process once local authentication is completed.
  void ResumeAfterLocalAuthentication(std::unique_ptr<UserContext>);
  // Invoked if login process was cancelled at local authentication.
  void OnLocalAuthenticationCancelled();

  // Returns name of the currently connected network, for error message,
  std::u16string GetConnectedNetworkName() const;

  // This is virtual for mocking in the unit tests.
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics);

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  void CompleteLogin(const UserContext& user_context);
  void OnGaiaScreenReady();
  void SetDisplayEmail(const std::string& email);
  bool IsUserAllowlisted(
      const AccountId& account_id,
      const std::optional<user_manager::UserType>& user_type);

  // This is virtual to be mocked in unit tests.
  virtual bool IsSigninInProgress() const;
  bool IsUserSigninCompleted() const;

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override;

  // HttpAuthDialog::Observer implementation:
  void HttpAuthDialogShown(content::WebContents* web_contents) override;
  void HttpAuthDialogCancelled(content::WebContents* web_contents) override;
  void HttpAuthDialogSupplied(content::WebContents* web_contents) override;

  // Add/remove a delegate that we will pass AuthStatusConsumer events to.
  void AddLoginStatusConsumer(AuthStatusConsumer* consumer);
  void RemoveLoginStatusConsumer(const AuthStatusConsumer* consumer);

  // Returns value of LoginPerformer::auth_mode() (cached if performer is
  // destroyed).
  LoginPerformer::AuthorizationMode auth_mode() const;

  // Returns value of LoginPerformer::password_changed() (cached if performer is
  // destroyed).
  bool password_changed() const;

  // Returns true if auto launch is scheduled and the timer is running.
  bool IsAutoLoginTimerRunningForTesting() const {
    return auto_login_timer_ && auto_login_timer_->IsRunning();
  }

  // Get account id used in last login attempt.
  AccountId GetLastLoginAttemptAccountId() const;

  // Extracts out users allowed on login screen.
  static user_manager::UserList ExtractLoginUsers(
      const user_manager::UserList& users);

  // Calls login() on previously-used `login_performer_`.
  void LoginAuthenticated(std::unique_ptr<UserContext> user_context);

 private:
  friend class ExistingUserControllerTest;
  friend class ExistingUserControllerAutoLoginTest;
  friend class ExistingUserControllerPublicSessionTest;
  friend class MockLoginPerformerDelegate;
  friend class ExistingUserControllerForcedOnlineAuthTest;

  FRIEND_TEST_ALL_PREFIXES(ExistingUserControllerTest, ExistingUserLogin);

  class DeviceLocalAccountPolicyWaiter;

  void LoginAsGuest();
  void LoginAsPublicSession(const UserContext& user_context);
  void LoginAsKioskApp(KioskAppId kiosk_app_id);
  // Retrieve public session auto-login policy and update the
  // timer.
  void ConfigureAutoLogin();

  // Trigger public session auto-login.
  void OnPublicSessionAutoLoginTimerFire();

  // LoginPerformer::Delegate implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnOnlinePasswordUnusable(std::unique_ptr<UserContext>,
                                bool online_password_mismatch) override;
  void OnLocalAuthenticationRequired(
      std::unique_ptr<UserContext> user_context) override;
  void OnOldEncryptionDetected(std::unique_ptr<UserContext>,
                               bool has_incomplete_migration) override;
  void AllowlistCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void ReportOnAuthSuccessMetrics() override;

  void OnOnlinePasswordUnusableImpl(std::unique_ptr<UserContext>,
                                    bool online_password_mismatch);

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;
  base::WeakPtr<UserSessionManagerDelegate> AsWeakPtr() override;

  // Called when device settings change.
  void DeviceSettingsChanged();

  // Show error message corresponding to `error`. If `details` string is not
  // empty, it specify additional error text provided by authenticator, it is
  // not localized.
  void ShowError(SigninError error, const std::string& details);

  // Shows privacy notification in case of auto lunch managed guest session.
  void ShowAutoLaunchManagedGuestSessionNotification();

  // Shows "filesystem encryption migration" screen.
  void ShowEncryptionMigrationScreen(std::unique_ptr<UserContext> user_context,
                                     EncryptionMigrationMode migration_mode);

  // Shows "critical TPM error" screen.
  void ShowTPMError();

  // Creates `login_performer_` if necessary and calls login() on it.
  void PerformLogin(const UserContext& user_context,
                    LoginPerformer::AuthorizationMode auth_mode);

  // Calls login() on previously-used `login_performer_`.
  void ContinuePerformLogin(LoginPerformer::AuthorizationMode auth_mode,
                            std::unique_ptr<UserContext> user_context);

  // Removes the constraint that user home mount requires ext4 encryption from
  // `user_context`, then calls login() on previously-used `login_performer`.
  void ContinuePerformLoginWithoutMigration(
      LoginPerformer::AuthorizationMode auth_mode,
      std::unique_ptr<UserContext> user_context);

  // Asks the user to enter their password again.
  void RestartLogin(const UserContext& user_context);

  // Updates the `login_display_` attached to this controller.
  void UpdateLoginDisplay(const user_manager::UserList& users);

  // Sends an accessibility alert event to extension listeners.
  void SendAccessibilityAlert(const std::string& alert_text);

  // Continues public session login if the public session policy is loaded.
  // This is intended to delay public session login if the login is requested
  // before the policy is available (in which case the login attempt would
  // fail).
  void LoginAsPublicSessionWhenPolicyAvailable(const UserContext& user_context);

  // Callback invoked when the keyboard layouts available for a public session
  // have been retrieved. Selects the first layout from the list and continues
  // login.
  void SetPublicSessionKeyboardLayoutAndLogin(
      const UserContext& user_context,
      base::Value::List keyboard_layouts);

  // Starts the actual login process for a public session. Invoked when all
  // preconditions have been verified.
  void LoginAsPublicSessionInternal(const UserContext& user_context);

  // Performs sets of actions right prior to login has been started.
  void PerformPreLoginActions(const UserContext& user_context);

  // Performs set of actions when login has been completed or has been
  // cancelled. If `start_auto_login_timer` is true than
  // auto-login timer is started.
  void PerformLoginFinishedActions(bool start_auto_login_timer);

  // Invokes `continuation` after verifying that cryptohome service is
  // available.
  void ContinueLoginWhenCryptohomeAvailable(base::OnceClosure continuation,
                                            bool service_is_available);

  // Invokes `continuation` after verifying that the device is not disabled.
  void ContinueLoginIfDeviceNotDisabled(base::OnceClosure continuation);

  // Signs in as a new user. This is a continuation of CompleteLogin() that gets
  // invoked after it has been verified that the device is not disabled.
  void DoCompleteLogin(const UserContext& user_context);

  // Signs in as a known user. This is a continuation of Login() that gets
  // invoked after it has been verified that the device is not disabled.
  void DoLogin(const UserContext& user_context,
               const SigninSpecifics& specifics);

  // Callback invoked when `oauth2_token_initializer_` has finished.
  void OnOAuth2TokensFetched(bool success, const UserContext& user_context);

  // Triggers online login for the given `account_id`.
  void ForceOnlineLoginForAccountId(const AccountId& account_id);

  // Clear the recorded displayed email, displayed name, given name so it won't
  // affect any future attempts.
  void ClearRecordedNames();

  // Public session auto-login timer.
  std::unique_ptr<base::OneShotTimer> auto_login_timer_;

  // Auto-login timeout, in milliseconds.
  int auto_login_delay_;

  // True if a profile has been prepared.
  bool profile_prepared_ = false;

  // AccountId for public session auto-login.
  AccountId public_session_auto_login_account_id_ = EmptyAccountId();

  // Used to execute login operations.
  std::unique_ptr<LoginPerformer> login_performer_;

  // Delegates to forward all authentication status events to.
  // Tests can use this to receive authentication status events.
  base::ObserverList<AuthStatusConsumer> auth_status_consumers_;

  // AccountId of the last login attempt.
  AccountId last_login_attempt_account_id_ = EmptyAccountId();

  // Whether the last login attempt was an auto login.
  bool last_login_attempt_was_auto_login_ = false;

  // Number of login attempts. Used to show help link when > 1 unsuccessful
  // logins for the same user.
  size_t num_login_attempts_ = 0;

  // Interface to the signed settings store.
  raw_ptr<CrosSettings> cros_settings_;

  // URL to append to start Guest mode with.
  GURL guest_mode_url_;

  // Once Lacros is shipped, this will no longer be necessary.
  std::unique_ptr<HttpAuthDialog::ScopedEnabler> enable_ash_httpauth_;

  // The displayed email for the next login attempt set by `SetDisplayEmail`.
  std::string display_email_;

  // Whether login attempt is running.
  bool is_login_in_progress_ = false;

  // Whether user signin is completed.
  bool is_signin_completed_ = false;

  // True if password has been changed for user who is completing sign in.
  // Set in OnLoginSuccess. Before that use LoginPerformer::password_changed().
  bool password_changed_ = false;

  // Set in OnLoginSuccess. Before that use LoginPerformer::auth_mode().
  // Initialized with `kExternal` as more restricted mode.
  LoginPerformer::AuthorizationMode auth_mode_ =
      LoginPerformer::AuthorizationMode::kExternal;

  // Timer when the signin screen was first displayed. Used to measure the time
  // from showing the screen until a successful login is performed.
  std::unique_ptr<base::ElapsedTimer> timer_init_;

  // Timer for the interval to wait for the reboot after TPM error UI was shown.
  base::OneShotTimer reboot_timer_;

  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  base::CallbackListSubscription show_user_names_subscription_;
  base::CallbackListSubscription allow_guest_subscription_;
  base::CallbackListSubscription users_subscription_;
  base::CallbackListSubscription local_account_auto_login_id_subscription_;
  base::CallbackListSubscription local_account_auto_login_delay_subscription_;
  base::CallbackListSubscription family_link_allowed_subscription_;

  std::unique_ptr<OAuth2TokenInitializer> oauth2_token_initializer_;

  // Used to wait for local account policy during session login, if policy is
  // not yet available when the login is attempted.
  std::unique_ptr<DeviceLocalAccountPolicyWaiter> policy_waiter_;

  // The source of PIN salts. Used to retrieve PIN during TransformPinKey.
  std::unique_ptr<quick_unlock::PinSaltStorage> pin_salt_storage_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observed_user_manager_{this};

  // Factory of callbacks.
  base::WeakPtrFactory<ExistingUserController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_
