// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"

namespace session_manager {
class SessionManager;
}

namespace chromeos {

class FakeLoginDisplayHost : public LoginDisplayHost {
 public:
  FakeLoginDisplayHost();
  ~FakeLoginDisplayHost() override;

  // chromeos::LoginDisplayHost:
  LoginDisplay* GetLoginDisplay() override;
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void BeforeSessionStart() override;
  void Finalize(base::OnceClosure) override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(chromeos::OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  AppLaunchController* GetAppLaunchController() override;
  void StartUserAdding(base::OnceClosure completion_callback) override;
  void CancelUserAdding() override;
  void StartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void PrewarmAuthentication() override;
  void StartAppLaunch(const std::string& app_id,
                      bool diagnostic_mode,
                      bool is_auto_launch) override;
  void StartDemoAppLaunch() override;
  void StartArcKiosk(const AccountId& account_id) override;
  void StartWebKiosk(const AccountId& account_id) override;
  void CompleteLogin(const chromeos::UserContext& user_context) override;
  void OnGaiaScreenReady() override;
  void SetDisplayEmail(const std::string& email) override;
  void SetDisplayAndGivenName(const std::string& display_name,
                              const std::string& given_name) override;
  void LoadWallpaper(const AccountId& account_id) override;
  void LoadSigninWallpaper() override;
  bool IsUserWhitelisted(const AccountId& account_id) override;
  void ShowGaiaDialog(bool can_close,
                      const AccountId& prefilled_account) override;
  void HideOobeDialog() override;
  void UpdateOobeDialogState(ash::OobeDialogState state) override;
  const user_manager::UserList GetUsers() override;
  void CancelPasswordChangedFlow() override;
  void MigrateUserData(const std::string& old_password) override;
  void ResyncUserData() override;
  void ShowFeedback() override;
  void ShowResetScreen() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;

 private:
  class FakeBaseScreen;

  // SessionManager is required by the constructor of WizardController.
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<FakeBaseScreen> fake_screen_;
  std::unique_ptr<WizardController> wizard_controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_
