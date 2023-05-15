// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/fake_login_display_host.h"

#include "base/notreached.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

class FakeLoginDisplayHost::FakeBaseScreen : public BaseScreen {
 public:
  explicit FakeBaseScreen(OobeScreenId screen_id)
      : BaseScreen(screen_id, OobeScreenPriority::DEFAULT) {}

  FakeBaseScreen(const FakeBaseScreen&) = delete;
  FakeBaseScreen& operator=(const FakeBaseScreen&) = delete;

  ~FakeBaseScreen() override = default;

 private:
  // BaseScreen:
  void ShowImpl() override {}
  void HideImpl() override {}
};

FakeLoginDisplayHost::FakeLoginDisplayHost()
    : wizard_context_(std::make_unique<WizardContext>()) {
  // Only one SessionManager can be instantiated at a time. Check to see if one
  // has already been instantiated before creating one.
  if (!session_manager::SessionManager::Get())
    session_manager_ = std::make_unique<session_manager::SessionManager>();
}

FakeLoginDisplayHost::~FakeLoginDisplayHost() = default;

ExistingUserController* FakeLoginDisplayHost::GetExistingUserController() {
  return nullptr;
}

gfx::NativeWindow FakeLoginDisplayHost::GetNativeWindow() const {
  return nullptr;
}

views::Widget* FakeLoginDisplayHost::GetLoginWindowWidget() const {
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

bool FakeLoginDisplayHost::IsFinalizing() {
  return false;
}

void FakeLoginDisplayHost::Finalize(base::OnceClosure) {}

void FakeLoginDisplayHost::FinalizeImmediately() {}

void FakeLoginDisplayHost::SetStatusAreaVisible(bool visible) {}

void FakeLoginDisplayHost::StartWizard(OobeScreenId first_screen) {
  wizard_controller_ =
      std::make_unique<WizardController>(wizard_context_.get());

  fake_screen_ = std::make_unique<FakeBaseScreen>(first_screen);
  wizard_controller_->SetCurrentScreenForTesting(fake_screen_.get());
}

WizardController* FakeLoginDisplayHost::GetWizardController() {
  return wizard_controller_.get();
}

KioskLaunchController* FakeLoginDisplayHost::GetKioskLaunchController() {
  return nullptr;
}

WizardContext* FakeLoginDisplayHost::GetWizardContext() {
  return nullptr;
}

void FakeLoginDisplayHost::StartUserAdding(
    base::OnceClosure completion_callback) {}

void FakeLoginDisplayHost::CancelUserAdding() {}

void FakeLoginDisplayHost::StartSignInScreen() {}

void FakeLoginDisplayHost::StartKiosk(const KioskAppId& kiosk_app_id,
                                      bool is_auto_launch) {}

void FakeLoginDisplayHost::AttemptShowEnableConsumerKioskScreen() {}

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
    const absl::optional<user_manager::UserType>& user_type) {
  return false;
}

void FakeLoginDisplayHost::ShowGaiaDialog(const AccountId& prefilled_account) {}

void FakeLoginDisplayHost::ShowAllowlistCheckFailedError() {}

void FakeLoginDisplayHost::ShowOsInstallScreen() {}

void FakeLoginDisplayHost::ShowGuestTosScreen() {}

void FakeLoginDisplayHost::HideOobeDialog(bool saml_page_closed) {}

void FakeLoginDisplayHost::SetShelfButtonsEnabled(bool enabled) {}

void FakeLoginDisplayHost::UpdateOobeDialogState(OobeDialogState state) {}

void FakeLoginDisplayHost::CancelPasswordChangedFlow() {}

void FakeLoginDisplayHost::MigrateUserData(const std::string& old_password) {}

void FakeLoginDisplayHost::ResyncUserData() {}

bool FakeLoginDisplayHost::HandleAccelerator(LoginAcceleratorAction action) {
  return false;
}

void FakeLoginDisplayHost::HandleDisplayCaptivePortal() {}

void FakeLoginDisplayHost::UpdateAddUserButtonStatus() {}

void FakeLoginDisplayHost::RequestSystemInfoUpdate() {}

bool FakeLoginDisplayHost::HasUserPods() {
  return false;
}

void FakeLoginDisplayHost::VerifyOwnerForKiosk(base::OnceClosure) {}

void FakeLoginDisplayHost::AddObserver(LoginDisplayHost::Observer* observer) {}

void FakeLoginDisplayHost::RemoveObserver(
    LoginDisplayHost::Observer* observer) {}

SigninUI* FakeLoginDisplayHost::GetSigninUI() {
  return nullptr;
}

void FakeLoginDisplayHost::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  NOTREACHED();
}

bool FakeLoginDisplayHost::IsWizardControllerCreated() const {
  return wizard_controller_.get();
}

WizardContext* FakeLoginDisplayHost::GetWizardContextForTesting() {
  NOTREACHED();
  return nullptr;
}

bool FakeLoginDisplayHost::IsWebUIStarted() const {
  return wizard_controller_.get();
}

base::WeakPtr<ash::quick_start::TargetDeviceBootstrapController>
FakeLoginDisplayHost::GetQuickStartBootstrapController() {
  return nullptr;
}

bool FakeLoginDisplayHost::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  return false;
}

}  // namespace ash
