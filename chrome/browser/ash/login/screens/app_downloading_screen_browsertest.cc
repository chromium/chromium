// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/app_downloading_screen.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr char kAppDownloadingId[] = "app-downloading";

const test::UIPath kTitle = {kAppDownloadingId, "title"};
const test::UIPath kContinueSetupButton = {kAppDownloadingId,
                                           "continue-setup-button"};

class AppDownloadingScreenTest : public OobeBaseTest {
 public:
  AppDownloadingScreenTest() = default;
  ~AppDownloadingScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    app_downloading_screen_ = WizardController::default_controller()
                                  ->GetScreen<AppDownloadingScreen>();
    app_downloading_screen_->set_exit_callback_for_testing(
        screen_exit_waiter_.GetRepeatingCallback());
  }

  void Login() {
    login_manager_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  }

  void ShowAppDownloadingScreen() {
    LoginDisplayHost::default_host()->StartWizard(
        AppDownloadingScreenView::kScreenId);
    OobeScreenWaiter(AppDownloadingScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() { EXPECT_TRUE(screen_exit_waiter_.Wait()); }

  raw_ptr<AppDownloadingScreen, DanglingUntriaged> app_downloading_screen_;
  bool screen_exited_ = false;

 private:
  base::test::TestFuture<void> screen_exit_waiter_;

  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, NoAppsSelected) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->defer_oobe_flow_finished_for_tests = true;

  Login();
  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitle);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, SingleAppSelected) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->defer_oobe_flow_finished_for_tests = true;

  Login();
  base::Value::List apps;
  apps.Append("app.test.package.1");

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetList(
      arc::prefs::kArcFastAppReinstallPackages, std::move(apps));
  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitle);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, MultipleAppsSelected) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->defer_oobe_flow_finished_for_tests = true;

  Login();
  base::Value::List apps;
  apps.Append("app.test.package.1");
  apps.Append("app.test.package.2");

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetList(
      arc::prefs::kArcFastAppReinstallPackages, std::move(apps));

  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitle);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

}  // namespace
}  // namespace ash
