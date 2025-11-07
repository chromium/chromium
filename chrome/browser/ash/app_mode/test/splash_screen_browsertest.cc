// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <vector>

#include "ash/public/cpp/shelf_config.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/check_deref.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_feedback.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/os_feedback_dialog/os_feedback_dialog.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::CurrentProfile;
using kiosk::test::IsAppInstalled;
using kiosk::test::LaunchAppManually;
using kiosk::test::OfflineEnabledChromeAppV2;
using kiosk::test::PressNetworkAccelerator;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;
using kiosk::test::WaitNetworkScreen;
using kiosk::test::WaitSplashScreen;

namespace {

const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

void ExpectNetworkScreenContinueButtonShown(bool is_shown) {
  test::OobeJS().ExpectPathDisplayed(is_shown,
                                     kNetworkConfigureScreenContinueButton);
}

void ClickNetworkScreenContinueButton() {
  test::OobeJS().ClickOnPath(kNetworkConfigureScreenContinueButton);
}

std::vector<KioskMixin::Config> SplashScreenTestConfigs() {
  // TODO(crbug.com/379633748): Add IWA.
  return {KioskMixin::Config{/*name=*/"WebApp",
                             /*auto_launch_account_id=*/{},
                             {KioskMixin::SimpleWebAppOption()}},
          KioskMixin::Config{/*name=*/"ChromeApp",
                             /*auto_launch_account_id=*/{},
                             {KioskMixin::SimpleChromeAppOption()}}};
}

std::vector<KioskMixin::Config> OfflineLaunchSplashScreenTestConfigs() {
  // TODO(crbug.com/379633748): Add IWA.
  return {KioskMixin::Config{/*name=*/"WebApp",
                             /*auto_launch_account_id=*/{},
                             {KioskMixin::SimpleWebAppOption()}},
          KioskMixin::Config{/*name=*/"ChromeApp",
                             /*auto_launch_account_id=*/{},
                             {OfflineEnabledChromeAppV2()}}};
}

SystemWebDialogDelegate* CreateFeedbackDialog() {
  auto login_feedback_ =
      std::make_unique<LoginFeedback>(ProfileHelper::Get()->GetSigninProfile());

  base::test::TestFuture<void> show_dialog_waiter;
  login_feedback_->Request(std::string(), show_dialog_waiter.GetCallback());
  EXPECT_TRUE(show_dialog_waiter.Wait());

  auto* dialog = SystemWebDialogDelegate::FindInstance(
      GURL{ash::kChromeUIOSFeedbackUrl}.spec());

  return dialog;
}

}  // namespace

class SplashScreenTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  SplashScreenTest() = default;
  SplashScreenTest(const SplashScreenTest&) = delete;
  SplashScreenTest& operator=(const SplashScreenTest&) = delete;

  ~SplashScreenTest() override = default;

  const KioskMixin::Config& config() { return GetParam(); }

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{&mixin_host_, /*cached_configuration=*/config()};
};

IN_PROC_BROWSER_TEST_P(SplashScreenTest, DisplaysNetworkScreenUntilOnline) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/false);

  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(SplashScreenTest, NetworkShortcutWorksOnline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitSplashScreen();

  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  scoped_launch_blocker.reset();
  ClickNetworkScreenContinueButton();
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(SplashScreenTest, CheckSuppressLoginAcceleratorActions) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitSplashScreen();

  // All actions are blocked except `kAppLaunchBailout` and
  // `kAppLaunchNetworkConfig`.
  std::vector<LoginAcceleratorAction> blocked_actions = {
      // kToggleSystemInfo is handled separately in
      // LoginDisplayHostMojo::HandleAccelerator,
      // before our kiosk logic has a chance to intercept this (which happens in
      // LoginDisplayHostCommon::HandleAccelerator).
      // Even so, letting that action slip through seems innocent enough.
      /*kToggleSystemInfo */
      kShowFeedback,          kShowResetScreen,         kCancelScreenAction,
      kStartEnrollment,       kStartKioskEnrollment,    kEnableDebugging,
      kEditDeviceRequisition, kDeviceRequisitionRemora, kStartDemoMode,
      kLaunchDiagnostics,     kEnableQuickStart,
  };

  for (auto action : blocked_actions) {
    // To suppress the action it is marked as handled.
    const bool is_handled =
        LoginDisplayHost::default_host()->HandleAccelerator(action);
    EXPECT_TRUE(is_handled)
        << "Action " << action << " is not marked as handled,"
        << " and thus will be processed by the next event target";
  }
}

IN_PROC_BROWSER_TEST_P(SplashScreenTest,
                       NoSystemWebDialogsExistAfterSplashScreen) {
  // Start offline to show the network screen and pause the launch.
  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitNetworkScreen();

  SystemWebDialogDelegate* dialog = CreateFeedbackDialog();
  ASSERT_TRUE(dialog);

  base::test::TestFuture<const std::string&> close_dialog_waiter;
  dialog->RegisterOnDialogClosedCallback(close_dialog_waiter.GetCallback());

  // Resume the app launch.
  network_state_.SimulateOnline();

  ASSERT_TRUE(WaitKioskLaunched());

  ASSERT_TRUE(close_dialog_waiter.Wait());

  // After the kiosk launch completed, the feedback dialog should be closed.
  ASSERT_FALSE(OsFeedbackDialog::FindDialogWindow());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SplashScreenTest,
                         testing::ValuesIn(SplashScreenTestConfigs()),
                         KioskMixin::ConfigName);

using OfflineLaunchEnabledSplashScreenTest = SplashScreenTest;

IN_PROC_BROWSER_TEST_P(OfflineLaunchEnabledSplashScreenTest,
                       PRE_NetworkShortcutWorksOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(OfflineLaunchEnabledSplashScreenTest,
                       NetworkShortcutWorksOffline) {
  network_state_.SimulateOffline();
  WaitNetworkScreen();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitSplashScreen();
  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  scoped_launch_blocker.reset();
  ClickNetworkScreenContinueButton();

  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OfflineLaunchEnabledSplashScreenTest,
    testing::ValuesIn(OfflineLaunchSplashScreenTestConfigs()),
    KioskMixin::ConfigName);

}  // namespace ash
