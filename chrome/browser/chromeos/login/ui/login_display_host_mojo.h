// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/chromeos/login/security_token_pin_dialog_host_ash_impl.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_common.h"
#include "chrome/browser/chromeos/login/ui/oobe_ui_dialog_delegate.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/challenge_response_key.h"

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
                             public AuthStatusConsumer,
                             public OobeUI::Observer {
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
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void OnStartAppLaunch() override;
  void OnStartArcKiosk() override;
  void OnStartWebKiosk() override;
  void OnBrowserCreated() override;
  void ShowGaiaDialog(bool can_close,
                      const AccountId& prefilled_account) override;
  void HideOobeDialog() override;
  void UpdateOobeDialogState(ash::OobeDialogState state) override;
  const user_manager::UserList GetUsers() override;
  void OnCancelPasswordChangedFlow() override;
  void ShowFeedback() override;
  void ShowResetScreen() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;

  // LoginScreenClient::Delegate:
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleEnrollUserWithExternalBinary(
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleAuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  void HandleOnNoPodFocused() override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnPasswordChangeDetected() override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

 private:
  void LoadOobeDialog();

  // Callback to be invoked when the |challenge_response_auth_keys_loader_|
  // completes building the currently available challenge-response keys. Used
  // only during the challenge-response authentication.
  void OnChallengeResponseKeysPrepared(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> on_auth_complete_callback,
      std::vector<ChallengeResponseKey> challenge_response_keys);

  // Helper methods to show and hide the dialog.
  void ShowDialog();
  void ShowFullScreen();
  void HideDialog();

  // Adds this as a |OobeUI::Observer| if it has not already been added as one.
  void ObserveOobeUI();

  // Removes this as a |OobeUI::Observer| if it has been added as an observer.
  void StopObservingOobeUI();

  // State associated with a pending authentication attempt.
  struct AuthState {
    AuthState(AccountId account_id, base::OnceCallback<void(bool)> callback);
    ~AuthState();

    // Account that is being authenticated.
    AccountId account_id;
    // Callback that should be executed the authentication result is available.
    base::OnceCallback<void(bool)> callback;
  };
  std::unique_ptr<AuthState> pending_auth_state_;

  std::unique_ptr<LoginDisplayMojo> login_display_;

  std::unique_ptr<UserBoardViewMojo> user_board_view_mojo_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;
  OobeUIDialogDelegate* dialog_ = nullptr;  // Not owned.
  bool can_close_dialog_ = true;
  std::unique_ptr<WizardController> wizard_controller_;

  // Users that are visible in the views login screen.
  // TODO(crbug.com/808277): consider remove user case.
  user_manager::UserList users_;

  // The account id of the user pod that's being focused.
  AccountId focused_pod_account_id_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  // Prevents repeated calls to OnStartSigninScreen, which can happen when a
  // user cancels the Powerwash dialog in the login screen. Set to true on the
  // first OnStartSigninScreen and remains true afterward.
  bool signin_screen_started_ = false;

  ChallengeResponseAuthKeysLoader challenge_response_auth_keys_loader_;

  SecurityTokenPinDialogHostAshImpl security_token_pin_dialog_host_ash_impl_;

  // Set if this has been added as a |OobeUI::Observer|.
  bool added_as_oobe_observer_ = false;

  base::WeakPtrFactory<LoginDisplayHostMojo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostMojo);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
