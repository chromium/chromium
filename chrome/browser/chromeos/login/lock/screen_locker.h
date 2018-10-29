// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/login_user_info.mojom.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace content {
class WebContents;
}

namespace chromeos {

class Authenticator;
class ExtendedAuthenticator;
class AuthFailure;
class ScreenlockIconProvider;
class WebUIScreenLocker;
class ViewsScreenLocker;

namespace test {
class ScreenLockerTester;
class ScreenLockerViewsTester;
class WebUIScreenLockerTester;
}  // namespace test

// ScreenLocker creates a WebUIScreenLocker which will display the lock UI.
// As well, it takes care of authenticating the user and managing a global
// instance of itself which will be deleted when the system is unlocked.
class ScreenLocker : public AuthStatusConsumer,
                     public device::mojom::FingerprintObserver {
 public:
  // Delegate used to send internal state changes back to the UI.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Enable/disable password input.
    virtual void SetPasswordInputEnabled(bool enabled) = 0;

    // Show the given error message.
    virtual void ShowErrorMessage(int error_msg_id,
                                  HelpAppLauncher::HelpTopic help_topic_id) = 0;

    // Close any displayed error messages.
    virtual void ClearErrors() = 0;

    // Run any visual effects after authentication is successful. This must call
    // ScreenLocker::UnlockOnLoginSuccess() after all effects are done.
    virtual void AnimateAuthenticationSuccess() = 0;

    // Called when the webui lock screen is ready. This gets invoked by a
    // chrome.send from the embedded webui.
    virtual void OnLockWebUIReady() = 0;

    // Called when webui lock screen wallpaper is loaded and displayed.
    virtual void OnLockBackgroundDisplayed() = 0;

    // Called when the webui header bar becomes visible.
    virtual void OnHeaderBarVisible() = 0;

    // Called by ScreenLocker to notify that ash lock animation finishes.
    virtual void OnAshLockAnimationFinished() = 0;

    // Called when fingerprint state has changed.
    virtual void SetFingerprintState(const AccountId& account_id,
                                     ash::mojom::FingerprintState state) = 0;

    // Called after a fingerprint authentication attempt.
    virtual void NotifyFingerprintAuthResult(const AccountId& account_id,
                                             bool success) = 0;

    // Returns the web contents used to back the lock screen.
    // TODO(jdufault): Remove this function when we remove WebUIScreenLocker.
    virtual content::WebContents* GetWebContents() = 0;

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

  // Does actual unlocking once authentication is successful and all blocking
  // animations are done.
  void UnlockOnLoginSuccess();

  // Authenticates the user with given |user_context|.
  void Authenticate(const UserContext& user_context,
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

  // Returns the WebUIScreenLocker instance. This should only be used in tests.
  // When using views-based lock this will be a nullptr.
  // TODO(jdufault): Remove this function, make tests agnostic to ui impl.
  WebUIScreenLocker* web_ui_for_testing() { return web_ui_.get(); }

  // Returns delegate that can be used to talk to the view-layer.
  Delegate* delegate() { return delegate_; }

  // Returns the users to authenticate.
  const user_manager::UserList& users() const { return users_; }

  // Allow a AuthStatusConsumer to listen for
  // the same login events that ScreenLocker does.
  void SetLoginStatusConsumer(chromeos::AuthStatusConsumer* consumer);

  // Initialize or uninitialize the ScreenLocker class. It listens to
  // NOTIFICATION_SESSION_STARTED so that the screen locker accepts lock
  // requests only after a user has logged in.
  static void InitClass();
  static void ShutDownClass();

  // Handles a request from the session manager to show the lock screen.
  static void HandleShowLockScreenRequest();

  // Show the screen locker.
  static void Show();

  // Hide the screen locker.
  static void Hide();

  // Returns the tester
  static test::ScreenLockerTester* GetTester();

  // Saves sync password hash and salt to user profile prefs based on
  // |user_context|.
  void SaveSyncPasswordHash(const UserContext& user_context);

 private:
  friend class base::DeleteHelper<ScreenLocker>;
  friend class test::ScreenLockerTester;
  friend class test::ScreenLockerViewsTester;
  friend class test::WebUIScreenLockerTester;
  friend class WebUIScreenLocker;
  friend class ViewsScreenLocker;

  // Track whether the user used pin or password to unlock the lock screen.
  // Values corrospond to UMA histograms, do not modify, or add or delete other
  // than directly before AUTH_COUNT.
  enum UnlockType { AUTH_PASSWORD = 0, AUTH_PIN, AUTH_FINGERPRINT, AUTH_COUNT };

  struct AuthenticationParametersCapture {
    UserContext user_context;
  };

  ~ScreenLocker() override;

  // fingerprint::mojom::FingerprintObserver:
  void OnAuthScanDone(
      uint32_t scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;
  void OnRestarted() override {}
  void OnEnrollScanDone(uint32_t scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override {}

  void OnFingerprintAuthFailure(const user_manager::User& user);

  // Sets the authenticator.
  void SetAuthenticator(Authenticator* authenticator);

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

  // WebUIScreenLocker instance in use.
  std::unique_ptr<WebUIScreenLocker> web_ui_;

  // Delegate used to talk to the view.
  Delegate* delegate_ = nullptr;

  // Users that can unlock the device.
  user_manager::UserList users_;

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

  // Callback to run, if any, when authentication is done.
  AuthenticateCallback on_auth_complete_;

  // Copy of parameters passed to last call of OnLoginSuccess for usage in
  // UnlockOnLoginSuccess().
  std::unique_ptr<AuthenticationParametersCapture> authentication_capture_;

  // Provider for button icon set by the screenlockPrivate API.
  std::unique_ptr<ScreenlockIconProvider> screenlock_icon_provider_;

  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state_;

  device::mojom::FingerprintPtr fp_service_;
  mojo::Binding<device::mojom::FingerprintObserver>
      fingerprint_observer_binding_;

  // ViewsScreenLocker instance in use.
  std::unique_ptr<ViewsScreenLocker> views_screen_locker_;

  // Password is required every 24 hours in order to use fingerprint unlock.
  // This is used to update fingerprint state when password is required.
  base::OneShotTimer update_fingerprint_state_timer_;

  base::WeakPtrFactory<ScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
