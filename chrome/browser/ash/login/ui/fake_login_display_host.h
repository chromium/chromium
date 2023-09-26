// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace session_manager {
class SessionManager;
}

namespace ash {

class FakeLoginDisplayHost : public LoginDisplayHost {
 public:
  FakeLoginDisplayHost();

  FakeLoginDisplayHost(const FakeLoginDisplayHost&) = delete;
  FakeLoginDisplayHost& operator=(const FakeLoginDisplayHost&) = delete;

  ~FakeLoginDisplayHost() override;

  // LoginDisplayHost:
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  views::Widget* GetLoginWindowWidget() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void BeforeSessionStart() override;
  bool IsFinalizing() override;
  void FinalizeImmediately() override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  KioskLaunchController* GetKioskLaunchController() override;
  void CancelUserAdding() override;
  void StartSignInScreen() override;
  void StartKiosk(const KioskAppId& kiosk_app_id, bool is_auto_launch) override;
  void AttemptShowEnableConsumerKioskScreen() override;
  void CompleteLogin(const UserContext& user_context) override;
  void OnGaiaScreenReady() override;
  void SetDisplayEmail(const std::string& email) override;
  void UpdateWallpaper(const AccountId& prefilled_account) override;
  bool IsUserAllowlisted(
      const AccountId& account_id,
      const absl::optional<user_manager::UserType>& user_type) override;
  void ShowGaiaDialog(const AccountId& prefilled_account) override;
  void ShowAllowlistCheckFailedError() override;
  void ShowOsInstallScreen() override;
  void ShowGuestTosScreen() override;
  void HideOobeDialog(bool saml_page_closed = false) override;
  void SetShelfButtonsEnabled(bool enabled) override;
  void UpdateOobeDialogState(OobeDialogState state) override;
  void CancelPasswordChangedFlow() override;
  void MigrateUserData(const std::string& old_password) override;
  void ResyncUserData() override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;
  bool HasUserPods() override;
  void VerifyOwnerForKiosk(base::OnceClosure on_success) override;
  void AddObserver(LoginDisplayHost::Observer* observer) override;
  void RemoveObserver(LoginDisplayHost::Observer* observer) override;
  WizardContext* GetWizardContext() override;
  SigninUI* GetSigninUI() override;
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* value) const final;
  void AddWizardCreatedObserverForTests(
      base::RepeatingClosure on_created) final;
  bool IsWizardControllerCreated() const final;
  WizardContext* GetWizardContextForTesting() final;
  bool IsWebUIStarted() const final;
  base::WeakPtr<ash::quick_start::TargetDeviceBootstrapController>
  GetQuickStartBootstrapController() final;

  void SetOobeUI(OobeUI* oobe_ui);
  void SetWizardController(std::unique_ptr<WizardController> wizard_controller);
  OobeMetricsHelper* GetOobeMetricsHelper() override;

 private:
  class FakeBaseScreen;

  raw_ptr<OobeUI> oobe_ui_ = nullptr;

  // SessionManager is required by the constructor of WizardController.
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<FakeBaseScreen> fake_screen_;
  std::unique_ptr<WizardContext> wizard_context_;
  std::unique_ptr<WizardController> wizard_controller_;
  std::unique_ptr<OobeMetricsHelper> oobe_metrics_helper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_FAKE_LOGIN_DISPLAY_HOST_H_
