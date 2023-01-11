// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_H_

#include <memory>
#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class AccountId;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ExistingUserController;
class KioskAppId;
class KioskLaunchController;
class OobeUI;
class WebUILoginView;
class WizardContext;
class WizardController;
enum class OobeDialogState;

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
  class Observer : public base::CheckedObserver {
   public:
    // `bounds` is the WebDialogView's bounds in screen coordinate system.
    virtual void WebDialogViewBoundsChanged(const gfx::Rect& bounds) = 0;
  };

  LoginDisplayHost(const LoginDisplayHost&) = delete;
  LoginDisplayHost& operator=(const LoginDisplayHost&) = delete;

  // Returns the default LoginDisplayHost instance if it has been created.
  static LoginDisplayHost* default_host() { return default_host_; }

  // Returns an unowned pointer to the LoginDisplay instance.
  virtual LoginDisplay* GetLoginDisplay() = 0;

  // Returns an unowned pointer to the ExistingUserController instance.
  virtual ExistingUserController* GetExistingUserController() = 0;

  // Returns corresponding native window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the current login window widget.
  virtual views::Widget* GetLoginWindowWidget() const = 0;

  // Returns instance of the OOBE WebUI.
  virtual OobeUI* GetOobeUI() const = 0;

  // Return the WebContents instance of OOBE, if any.
  virtual content::WebContents* GetOobeWebContents() const = 0;

  // Returns the current login view.
  virtual WebUILoginView* GetWebUILoginView() const = 0;

  // Called when browsing session starts before creating initial browser.
  virtual void BeforeSessionStart() = 0;

  // Whether the process of deleting LoginDisplayHost has been started.
  virtual bool IsFinalizing() = 0;

  // Called when user enters or returns to browsing session so LoginDisplayHost
  // instance may delete itself. `completion_callback` will be invoked when the
  // instance is gone.
  virtual void Finalize(base::OnceClosure completion_callback) = 0;

  // Called when current instance should be replaced with another one. After the
  // call the instance will be gone.
  virtual void FinalizeImmediately() = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with `first_screen`.
  virtual void StartWizard(OobeScreenId first_screen) = 0;

  // Returns current WizardController, if it exists.
  // Result should not be stored.
  virtual WizardController* GetWizardController() = 0;

  virtual WizardContext* GetWizardContext() = 0;

  // Returns current KioskLaunchController, if it exists.
  // Result should not be stored.
  virtual KioskLaunchController* GetKioskLaunchController() = 0;

  // Starts screen for adding user into session.
  // `completion_callback` is invoked after login display host shutdown.
  // `completion_callback` can be null.
  virtual void StartUserAdding(base::OnceClosure completion_callback) = 0;

  // Cancel addint user into session.
  virtual void CancelUserAdding() = 0;

  // Starts sign in screen.
  virtual void StartSignInScreen() = 0;

  // Start kiosk identified by `kiosk_app_id` splash screen. if `is_auto_launch`
  // is true, the app is being auto-launched with no delay.
  virtual void StartKiosk(const KioskAppId& kiosk_app_id,
                          bool is_auto_launch) = 0;

  // Performs necessary check and shows consumer kiosk UI if eligible.
  virtual void AttemptShowEnableConsumerKioskScreen() = 0;

  // Show the gaia dialog. If available, `account` is preloaded in the gaia
  // dialog.
  virtual void ShowGaiaDialog(const AccountId& prefilled_account) = 0;

  // Whether the gaia dialog is visible.
  virtual bool IsGaiaDialogVisibleForTesting() = 0;

  // Show allowlist check failed error. Happens after user completes online
  // signin but allowlist check fails.
  virtual void ShowAllowlistCheckFailedError() = 0;

  // Show the os install dialog.
  virtual void ShowOsInstallScreen() = 0;

  // Show the guest terms of service screen.
  virtual void ShowGuestTosScreen() = 0;

  // Hide any visible oobe dialog.
  virtual void HideOobeDialog(bool saml_page_closed = false) = 0;

  // Sets whether shelf buttons are enabled.
  virtual void SetShelfButtonsEnabled(bool enabled) = 0;

  // Update the state of the oobe dialog.
  virtual void UpdateOobeDialogState(OobeDialogState state) = 0;

  // Confirms sign in by provided credentials in `user_context`.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const UserContext& user_context) = 0;

  // Notify the backend controller when the GAIA UI is finished loading.
  virtual void OnGaiaScreenReady() = 0;

  // Sets the displayed email for the next login attempt. If it succeeds,
  // user's displayed email value will be updated to `email`.
  virtual void SetDisplayEmail(const std::string& email) = 0;

  // Sets the displayed name and given name for the next login attempt. If it
  // succeeds, user's displayed name and give name values will be updated to
  // `display_name` and `given_name`.
  virtual void SetDisplayAndGivenName(const std::string& display_name,
                                      const std::string& given_name) = 0;

  // Load wallpaper for given `account_id`.
  virtual void LoadWallpaper(const AccountId& account_id) = 0;

  // Loads the default sign-in wallpaper.
  virtual void LoadSigninWallpaper() = 0;

  // Returns true if user is allowed to log in by domain policy.
  virtual bool IsUserAllowlisted(
      const AccountId& account_id,
      const absl::optional<user_manager::UserType>& user_type) = 0;

  // ----- Password change flow methods -----
  // Cancels current password changed flow.
  virtual void CancelPasswordChangedFlow() = 0;

  // Decrypt cryptohome using user provided `old_password` and migrate to new
  // password.
  virtual void MigrateUserData(const std::string& old_password) = 0;

  // Ignore password change, remove existing cryptohome and force full sync of
  // user data.
  virtual void ResyncUserData() = 0;

  // Handles an accelerator action.
  // Returns `true` if the accelerator was handled.
  virtual bool HandleAccelerator(LoginAcceleratorAction action) = 0;

  // Handles a request to show the captive portal web dialog. For webui, the
  // dialog is displayed immediately. For views, the dialog is displayed as soon
  // as the OOBE dialog is visible.
  virtual void HandleDisplayCaptivePortal() = 0;

  // Update status of add user button in the shelf.
  virtual void UpdateAddUserButtonStatus() = 0;

  // Update the system info at login screen.
  virtual void RequestSystemInfoUpdate() = 0;

  // Returns if the device has any user after filtering based on policy.
  virtual bool HasUserPods() = 0;

  virtual void VerifyOwnerForKiosk(base::OnceClosure on_success) = 0;

  // Used to add an observer for the changes in the web dilaog login view.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Return sign-in UI instance during OOBE/Login process.
  // Result should not be stored.
  virtual SigninUI* GetSigninUI() = 0;

  // Gets the keyboard remapped pref value for `pref_name` key. Returns true if
  // successful, otherwise returns false.
  // It provides a remapping based on currently selected user pod (as different
  // users might have different remappings).
  virtual bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                            int* value) const = 0;
  // Allows tests to wait for WebUI to start.
  // RepeatingClosure type matches base::RunLoop::QuitClosure result type.
  virtual void AddWizardCreatedObserverForTests(
      base::RepeatingClosure on_created) = 0;

  // Returns true if WizardController was created.
  virtual bool IsWizardControllerCreated() const = 0;

  // Returns pointer to the WizardContext for tests.
  virtual WizardContext* GetWizardContextForTesting() = 0;

  // Returns true if WebUI was created, which allows observers to wait for
  // Browser initialization finish.
  virtual bool IsWebUIStarted() const = 0;

  virtual base::WeakPtr<ash::quick_start::TargetDeviceBootstrapController>
  GetQuickStartBootstrapController() = 0;

 protected:
  LoginDisplayHost();
  virtual ~LoginDisplayHost();

  // Triggers |on_wizard_controller_created_for_tests_| callback.
  void NotifyWizardCreated();

 private:
  // Global LoginDisplayHost instance.
  static LoginDisplayHost* default_host_;

  // Callback to be executed when WebUI is started.
  base::RepeatingClosure on_wizard_controller_created_for_tests_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
