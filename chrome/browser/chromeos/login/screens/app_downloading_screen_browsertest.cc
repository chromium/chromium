// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/app_downloading_screen.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kAppDownloadingId[] = "app-downloading";

const test::UIPath kTitlePlural = {kAppDownloadingId, "title-plural"};
const test::UIPath kTitleSingular = {kAppDownloadingId, "title-singular"};
const test::UIPath kContinueSetupButton = {kAppDownloadingId,
                                           "continue-setup-button"};

}  // namespace

class AppDownloadingScreenTest : public OobeBaseTest {
 public:
  AppDownloadingScreenTest() = default;
  ~AppDownloadingScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    app_downloading_screen_ = AppDownloadingScreen::Get(
        WizardController::default_controller()->screen_manager());
    app_downloading_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &AppDownloadingScreenTest::HandleScreenExit, base::Unretained(this)));
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

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  AppDownloadingScreen* app_downloading_screen_;
  bool screen_exited_ = false;

 private:
  void HandleScreenExit() {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::OnceClosure screen_exit_callback_;

  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, NoAppsSelected) {
  Login();
  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitlePlural);
  test::OobeJS().ExpectHiddenPath(kTitleSingular);

  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE_PLURAL,
                                base::ASCIIToUTF16("0")),
      kTitlePlural);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, SingleAppSelected) {
  Login();
  base::Value apps(base::Value::Type::LIST);
  apps.Append("app.test.package.1");

  ProfileManager::GetActiveUserProfile()->GetPrefs()->Set(
      arc::prefs::kArcFastAppReinstallPackages, std::move(apps));
  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitleSingular);
  test::OobeJS().ExpectHiddenPath(kTitlePlural);

  test::OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE_SINGULAR),
      kTitleSingular);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(AppDownloadingScreenTest, MultipleAppsSelected) {
  Login();
  base::Value apps(base::Value::Type::LIST);
  apps.Append("app.test.package.1");
  apps.Append("app.test.package.2");

  ProfileManager::GetActiveUserProfile()->GetPrefs()->Set(
      arc::prefs::kArcFastAppReinstallPackages, std::move(apps));

  ShowAppDownloadingScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kContinueSetupButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kContinueSetupButton);

  test::OobeJS().ExpectVisiblePath(kTitlePlural);
  test::OobeJS().ExpectHiddenPath(kTitleSingular);

  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE_PLURAL,
                                base::ASCIIToUTF16("2")),
      kTitlePlural);

  test::OobeJS().TapOnPath(kContinueSetupButton);

  WaitForScreenExit();
}

}  // namespace chromeos
