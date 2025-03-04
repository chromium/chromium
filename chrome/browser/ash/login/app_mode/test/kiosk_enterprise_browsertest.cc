// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const test::UIPath kErrorMessageContinueButton = {"error-message",
                                                  "continueButton"};

void PressConfigureNetworkAccelerator() {
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchNetworkConfig);
}

void WaitForOobeScreen(OobeScreenId screen) {
  OobeScreenWaiter(screen).Wait();
}

void WaitForNetworkScreen() {
  WaitForOobeScreen(ErrorScreenView::kScreenId);
}

}  // namespace

// Kiosk tests with fake enterprise enroll setup.
class KioskEnterpriseTest : public KioskBaseTest {
 public:
  KioskEnterpriseTest(const KioskEnterpriseTest&) = delete;
  KioskEnterpriseTest& operator=(const KioskEnterpriseTest&) = delete;

 protected:
  KioskEnterpriseTest() = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest,
                       HittingNetworkAcceleratorShouldShowNetworkScreen) {
  auto auto_reset = NetworkUiController::SetCanConfigureNetworkForTesting(true);

  // Block app loading until the welcome screen is shown.
  BlockAppLaunch(true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  PressConfigureNetworkAccelerator();

  WaitForNetworkScreen();

  // Continue button should be visible since we are online.
  EXPECT_TRUE(test::OobeJS().IsVisible(kErrorMessageContinueButton));

  // Let app launching resume.
  BlockAppLaunch(false);

  // Click on [Continue] button.
  test::OobeJS().TapOnPath(kErrorMessageContinueButton);

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(
    KioskEnterpriseTest,
    LaunchingAppThatRequiresNetworkWhilstOfflineShouldShowNetworkScreen) {
  auto auto_reset = NetworkUiController::SetCanConfigureNetworkForTesting(true);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(NetworkStatus::kPortal);

  WaitForNetworkScreen();

  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, LaunchAppUserCancel) {
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  // Do not let the app be run to avoid race condition.
  BlockAppLaunch(true);

  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  base::test::TestFuture<void> termination_future_;
  auto subscription = browser_shutdown::AddAppTerminatingCallback(
      termination_future_.GetCallback());
  settings_helper_.SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, true);

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchBailout);
  EXPECT_TRUE(termination_future_.Wait());

  EXPECT_EQ(KioskAppLaunchError::Error::kUserCancel,
            KioskAppLaunchError::Get());
}

}  // namespace ash
