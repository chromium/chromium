// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/ui/kiosk_app_menu_updater.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_common.h"
#include "chrome/browser/chromeos/login/ui/oobe_ui_dialog_delegate.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chromeos/login/auth/auth_status_consumer.h"

namespace chromeos {

class ExistingUserController;
class LoginDisplayMojo;
class OobeUIDialogDelegate;
class UserBoardViewMojo;
class UserSelectionScreen;
class MojoSystemInfoDispatcher;

// A LoginDisplayHost instance that sends requests to the views-based signin
// screen.
class LoginDisplayHostMojo : public LoginDisplayHostCommon,
                             public LoginScreenClient::Delegate,
                             public AuthStatusConsumer {
 public:
  LoginDisplayHostMojo();
  ~LoginDisplayHostMojo() override;

  // Called when the gaia dialog is destroyed.
  void OnDialogDestroyed(const OobeUIDialogDelegate* dialog);

  // Set the users in the views login screen.
  void SetUsers(const user_manager::UserList& users);

  // Show password changed dialog. If |show_password_error| is true, user
  // already tried to enter old password but it turned out to be incorrect.
  void ShowPasswordChangedDialog(bool show_password_error,
                                 const std::string& email);

  // Show whitelist check failed error. Happens after user completes online
  // signin but whitelist check fails.
  void ShowWhitelistCheckFailedError();

  // Show unrecoverable cryptohome error dialog.
  void ShowUnrecoverableCrypthomeErrorDialog();

  // Displays detailed error screen for error with ID |error_id|.
  void ShowErrorScreen(LoginDisplay::SigninError error_id);

  // Shows signin UI with specified email.
  void ShowSigninUI(const std::string& email);

  UserSelectionScreen* user_selection_screen() {
    return user_selection_screen_.get();
  }

  // LoginDisplayHost:
  LoginDisplay* GetLoginDisplay() override;
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void OnFinalize() override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreen first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void OnStartAppLaunch() override;
  void OnStartArcKiosk() override;
  void OnBrowserCreated() override;
  void StartVoiceInteractionOobe() override;
  bool IsVoiceInteractionOobe() override;
  void ShowGaiaDialog(
      bool can_close,
      const base::Optional<AccountId>& prefilled_account) override;
  void HideOobeDialog() override;
  void UpdateOobeDialogSize(int width, int height) override;
  void UpdateOobeDialogState(ash::mojom::OobeDialogState state) override;
  const user_manager::UserList GetUsers() override;
  void OnCancelPasswordChangedFlow() override;
  void ShowFeedback() override;
  void ShowResetScreen() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;

  // LoginScreenClient::Delegate:
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      AuthenticateUserWithPasswordOrPinCallback callback) override;
  void HandleAuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      AuthenticateUserWithExternalBinaryCallback callback) override;
  void HandleEnrollUserWithExternalBinary(
      EnrollUserWithExternalBinaryCallback callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  void HandleOnNoPodFocused() override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLoginAsGuest() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  void LoadOobeDialog();

  // State associated with a pending authentication attempt.
  struct AuthState {
    AuthState(AccountId account_id,
              AuthenticateUserWithPasswordOrPinCallback callback);
    ~AuthState();

    // Account that is being authenticated.
    AccountId account_id;
    // Callback that should be executed the authentication result is available.
    AuthenticateUserWithPasswordOrPinCallback callback;
  };
  std::unique_ptr<AuthState> pending_auth_state_;

  std::unique_ptr<LoginDisplayMojo> login_display_;

  std::unique_ptr<UserBoardViewMojo> user_board_view_mojo_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;
  OobeUIDialogDelegate* dialog_ = nullptr;
  bool can_close_dialog_ = true;
  std::unique_ptr<WizardController> wizard_controller_;

  // Users that are visible in the views login screen.
  // TODO(crbug.com/808277): consider remove user case.
  user_manager::UserList users_;

  // The account id of the user pod that's being focused.
  AccountId focused_pod_account_id_;

  KioskAppMenuUpdater kiosk_updater_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  // Prevents repeated calls to OnStartSigninScreen, which can happen when a
  // user cancels the Powerwash dialog in the login screen. Set to true on the
  // first OnStartSigninScreen and remains true afterward.
  bool signin_screen_started_ = false;

  base::WeakPtrFactory<LoginDisplayHostMojo> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostMojo);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
