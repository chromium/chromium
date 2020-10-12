// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/fake_login_display_host.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

class FakeLoginDisplayHost::FakeBaseScreen : public chromeos::BaseScreen {
 public:
  explicit FakeBaseScreen(chromeos::OobeScreenId screen_id)
      : BaseScreen(screen_id, OobeScreenPriority::DEFAULT) {}

  ~FakeBaseScreen() override = default;

 private:
  // chromeos::BaseScreen:
  void ShowImpl() override {}
  void HideImpl() override {}

  DISALLOW_COPY_AND_ASSIGN(FakeBaseScreen);
};

FakeLoginDisplayHost::FakeLoginDisplayHost()
    : session_manager_(std::make_unique<session_manager::SessionManager>()) {}

FakeLoginDisplayHost::~FakeLoginDisplayHost() = default;

LoginDisplay* FakeLoginDisplayHost::GetLoginDisplay() {
  return nullptr;
}

ExistingUserController* FakeLoginDisplayHost::GetExistingUserController() {
  return nullptr;
}

gfx::NativeWindow FakeLoginDisplayHost::GetNativeWindow() const {
  return nullptr;
}

OobeUI* FakeLoginDisplayHost::GetOobeUI() const {
  return nullptr;
}

content::WebContents* FakeLoginDisplayHost::GetOobeWebContents() const {
  return nullptr;
}

WebUILoginView* FakeLoginDisplayHost::GetWebUILoginView() const {
  return nullptr;
}

void FakeLoginDisplayHost::BeforeSessionStart() {}

void FakeLoginDisplayHost::Finalize(base::OnceClosure) {}

void FakeLoginDisplayHost::FinalizeImmediately() {}

void FakeLoginDisplayHost::SetStatusAreaVisible(bool visible) {}

void FakeLoginDisplayHost::StartWizard(OobeScreenId first_screen) {
  wizard_controller_ = std::make_unique<WizardController>();

  fake_screen_ = std::make_unique<FakeBaseScreen>(first_screen);
  wizard_controller_->SetCurrentScreenForTesting(fake_screen_.get());
}

WizardController* FakeLoginDisplayHost::GetWizardController() {
  return wizard_controller_.get();
}

KioskLaunchController* FakeLoginDisplayHost::GetKioskLaunchController() {
  return nullptr;
}

void FakeLoginDisplayHost::StartUserAdding(
    base::OnceClosure completion_callback) {}

void FakeLoginDisplayHost::CancelUserAdding() {}

void FakeLoginDisplayHost::StartSignInScreen() {}

void FakeLoginDisplayHost::OnPreferencesChanged() {}

void FakeLoginDisplayHost::PrewarmAuthentication() {}

void FakeLoginDisplayHost::StartDemoAppLaunch() {}

void FakeLoginDisplayHost::StartKiosk(const KioskAppId& kiosk_app_id,
                                      bool is_auto_launch) {}

void FakeLoginDisplayHost::CompleteLogin(const UserContext& user_context) {}

void FakeLoginDisplayHost::OnGaiaScreenReady() {}

void FakeLoginDisplayHost::SetDisplayEmail(const std::string& email) {}

void FakeLoginDisplayHost::SetDisplayAndGivenName(
    const std::string& display_name,
    const std::string& given_name) {}

void FakeLoginDisplayHost::LoadWallpaper(const AccountId& account_id) {}

void FakeLoginDisplayHost::LoadSigninWallpaper() {}

bool FakeLoginDisplayHost::IsUserAllowlisted(
    const AccountId& account_id,
    const base::Optional<user_manager::UserType>& user_type) {
  return false;
}

void FakeLoginDisplayHost::ShowGaiaDialog(const AccountId& prefilled_account) {}

void FakeLoginDisplayHost::HideOobeDialog() {}

void FakeLoginDisplayHost::UpdateOobeDialogState(ash::OobeDialogState state) {}

void FakeLoginDisplayHost::CancelPasswordChangedFlow() {}

void FakeLoginDisplayHost::MigrateUserData(const std::string& old_password) {}

void FakeLoginDisplayHost::ResyncUserData() {}

bool FakeLoginDisplayHost::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  return false;
}

void FakeLoginDisplayHost::HandleDisplayCaptivePortal() {}

void FakeLoginDisplayHost::UpdateAddUserButtonStatus() {}

void FakeLoginDisplayHost::RequestSystemInfoUpdate() {}

}  // namespace chromeos
