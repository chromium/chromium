// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "ui/gfx/native_widget_types.h"

class AccountId;

namespace ash {
enum class OobeDialogState;
}

namespace content {
class WebContents;
}

namespace chromeos {

class AppLaunchController;
class ExistingUserController;
class LoginScreenContext;
class OobeUI;
class WebUILoginView;
class WizardController;

// An interface that defines an out-of-box-experience (OOBE) or login screen
// host. It contains code specific to the login UI implementation.
//
// The inheritance graph is as folllows:
//
//                               LoginDisplayHost
//                                   /       |
//                LoginDisplayHostCommon   MockLoginDisplayHost
//                      /      |
//   LoginDisplayHostMojo    LoginDisplayHostWebUI
//
//
// - LoginDisplayHost defines the generic interface.
// - LoginDisplayHostCommon is UI-agnostic code shared between the views and
//   webui hosts.
// - MockLoginDisplayHost is for tests.
// - LoginDisplayHostMojo is for the login screen which is implemented in Ash.
//   TODO(estade): rename LoginDisplayHostMojo since it no longer uses Mojo.
// - LoginDisplayHostWebUI is for OOBE, which is written in HTML/JS/CSS.
class LoginDisplayHost {
 public:
  // Returns the default LoginDisplayHost instance if it has been created.
  static LoginDisplayHost* default_host() { return default_host_; }

  // Returns an unowned pointer to the LoginDisplay instance.
  virtual LoginDisplay* GetLoginDisplay() = 0;

  // Returns an unowned pointer to the ExistingUserController instance.
  virtual ExistingUserController* GetExistingUserController() = 0;

  // Returns corresponding native window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns instance of the OOBE WebUI.
  virtual OobeUI* GetOobeUI() const = 0;

  // Return the WebContents instance of OOBE, if any.
  virtual content::WebContents* GetOobeWebContents() const = 0;

  // Returns the current login view.
  virtual WebUILoginView* GetWebUILoginView() const = 0;

  // Called when browsing session starts before creating initial browser.
  virtual void BeforeSessionStart() = 0;

  // Called when user enters or returns to browsing session so LoginDisplayHost
  // instance may delete itself. |completion_callback| will be invoked when the
  // instance is gone.
  virtual void Finalize(base::OnceClosure completion_callback) = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with |first_screen|.
  virtual void StartWizard(OobeScreenId first_screen) = 0;

  // Returns current WizardController, if it exists.
  // Result should not be stored.
  virtual WizardController* GetWizardController() = 0;

  // Returns current AppLaunchController, if it exists.
  // Result should not be stored.
  virtual AppLaunchController* GetAppLaunchController() = 0;

  // Starts screen for adding user into session.
  // |completion_callback| is invoked after login display host shutdown.
  // |completion_callback| can be null.
  virtual void StartUserAdding(base::OnceClosure completion_callback) = 0;

  // Cancel addint user into session.
  virtual void CancelUserAdding() = 0;

  // Starts sign in screen.
  virtual void StartSignInScreen(const LoginScreenContext& context) = 0;

  // Invoked when system preferences that affect the signin screen have changed.
  virtual void OnPreferencesChanged() = 0;

  // Initiates authentication network prewarming.
  virtual void PrewarmAuthentication() = 0;

  // Starts app launch splash screen. If |is_auto_launch| is true, the app is
  // being auto-launched with no delay.
  virtual void StartAppLaunch(const std::string& app_id,
                              bool diagnostic_mode,
                              bool is_auto_launch) = 0;

  // Starts the demo app launch.
  virtual void StartDemoAppLaunch() = 0;

  // Starts ARC kiosk splash screen.
  virtual void StartArcKiosk(const AccountId& account_id) = 0;

  // Starts web kiosk splash screen.
  virtual void StartWebKiosk(const AccountId& account_id) = 0;

  // Show the gaia dialog. |can_close| determines if the user is allowed to
  // close the dialog. If available, |account| is preloaded in the gaia dialog.
  virtual void ShowGaiaDialog(bool can_close,
                              const AccountId& prefilled_account) = 0;

  // Hide any visible oobe dialog.
  virtual void HideOobeDialog() = 0;

  // Update the state of the oobe dialog.
  virtual void UpdateOobeDialogState(ash::OobeDialogState state) = 0;

  // Get users that are visible in the login screen UI.
  // This is mainly used by views login screen. WebUI login screen will
  // return an empty list.
  // TODO(crbug.com/808271): WebUI and views implementation should return the
  // same user list.
  virtual const user_manager::UserList GetUsers() = 0;

  // Confirms sign in by provided credentials in |user_context|.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const UserContext& user_context) = 0;

  // Notify the backend controller when the GAIA UI is finished loading.
  virtual void OnGaiaScreenReady() = 0;

  // Sets the displayed email for the next login attempt. If it succeeds,
  // user's displayed email value will be updated to |email|.
  virtual void SetDisplayEmail(const std::string& email) = 0;

  // Sets the displayed name and given name for the next login attempt. If it
  // succeeds, user's displayed name and give name values will be updated to
  // |display_name| and |given_name|.
  virtual void SetDisplayAndGivenName(const std::string& display_name,
                                      const std::string& given_name) = 0;

  // Load wallpaper for given |account_id|.
  virtual void LoadWallpaper(const AccountId& account_id) = 0;

  // Loads the default sign-in wallpaper.
  virtual void LoadSigninWallpaper() = 0;

  // Returns true if user is allowed to log in by domain policy.
  virtual bool IsUserWhitelisted(const AccountId& account_id) = 0;

  // ----- Password change flow methods -----
  // Cancels current password changed flow.
  virtual void CancelPasswordChangedFlow() = 0;

  // Decrypt cryptohome using user provided |old_password| and migrate to new
  // password.
  virtual void MigrateUserData(const std::string& old_password) = 0;

  // Ignore password change, remove existing cryptohome and force full sync of
  // user data.
  virtual void ResyncUserData() = 0;

  // Shows a feedback report dialog.
  virtual void ShowFeedback() = 0;

  // Shows the powerwash dialog.
  virtual void ShowResetScreen() = 0;

  // Handles a request to show the captive portal web dialog. For webui, the
  // dialog is displayed immediately. For views, the dialog is displayed as soon
  // as the OOBE dialog is visible.
  virtual void HandleDisplayCaptivePortal() = 0;

  // Update status of add user button in the shelf.
  virtual void UpdateAddUserButtonStatus() = 0;

  // Update the system info at login screen.
  virtual void RequestSystemInfoUpdate() = 0;

 protected:
  LoginDisplayHost();
  virtual ~LoginDisplayHost();

 private:
  // Global LoginDisplayHost instance.
  static LoginDisplayHost* default_host_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
