// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kAppLaunchSplashScreenId[] = "app-launch-splash";
constexpr char kDefaultNetwork[] = "default-network";
constexpr char kErrorScreenId[] = "error-message";

class FakeScopedDelegate : public AppLaunchSplashScreen::Delegate {
 public:
  explicit FakeScopedDelegate(AppLaunchSplashScreen* screen)
      : screen_(*screen) {
    screen_->SetDelegate(this);
  }

  ~FakeScopedDelegate() { screen_->SetDelegate(nullptr); }

  // AppLaunchSplashScreen::Delegate:
  void OnNetworkConfigFinished() override {
    network_config_finished_called_ = true;
  }

  bool network_config_finished_called() const {
    return network_config_finished_called_;
  }

 private:
  raw_ref<AppLaunchSplashScreen> screen_;
  bool network_config_finished_called_ = false;
};

void ShowAppLaunchSplashScreen() {
  WizardController::default_controller()->AdvanceToScreen(
      AppLaunchSplashScreenView::kScreenId);
}

AppLaunchSplashScreen* GetScreen() {
  return WizardController::default_controller()
      ->GetScreen<AppLaunchSplashScreen>();
}
}  // namespace

using AppLaunchSplashScreenBrowserTest = OobeBaseTest;

IN_PROC_BROWSER_TEST_F(AppLaunchSplashScreenBrowserTest, DisplaysAppData) {
  ShowAppLaunchSplashScreen();
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisible(kAppLaunchSplashScreenId);

  constexpr char kTestAppName[] = "Test App";
  constexpr char kTestAppUrl[] = "http://example.com/";

  AppLaunchSplashScreen::Data data(kTestAppName, gfx::ImageSkia(),
                                   GURL(kTestAppUrl));
  GetScreen()->SetAppData(std::move(data));

  test::OobeJS().ExpectElementText(kTestAppName,
                                   {kAppLaunchSplashScreenId, "appName"});
  test::OobeJS().ExpectElementText(kTestAppUrl,
                                   {kAppLaunchSplashScreenId, "appUrl"});
}

IN_PROC_BROWSER_TEST_F(AppLaunchSplashScreenBrowserTest, ShowsNetworkConfig) {
  ShowAppLaunchSplashScreen();
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisible(kAppLaunchSplashScreenId);

  GetScreen()->ShowNetworkConfigureUI(NetworkStateInformer::ONLINE,
                                      kDefaultNetwork);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisible(kErrorScreenId);
}

IN_PROC_BROWSER_TEST_F(AppLaunchSplashScreenBrowserTest,
                       DismissesNetworkConfig) {
  FakeScopedDelegate delegate(GetScreen());

  ShowAppLaunchSplashScreen();
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisible(kAppLaunchSplashScreenId);

  GetScreen()->ShowNetworkConfigureUI(NetworkStateInformer::ONLINE,
                                      kDefaultNetwork);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  test::OobeJS().ClickOnPath({kErrorScreenId, "continueButton"});

  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHidden(kErrorScreenId);
  EXPECT_TRUE(delegate.network_config_finished_called());
}

}  // namespace ash
