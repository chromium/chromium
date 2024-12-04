// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/public/cpp/shelf_config.h"
#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::CurrentProfile;
using kiosk::test::IsAppInstalled;
using kiosk::test::PressNetworkAccelerator;
using kiosk::test::TheKioskApp;
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

// Disables the Gaia screen offline message. Leaving this enabled may interfere
// with checks done in offline Kiosk launch tests, since it influences the
// screens `WizardController` shows.
void DisableGaiaOfflineScreen() {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetHandler<GaiaScreenHandler>()
      ->set_offline_timeout_for_testing(base::TimeDelta::Max());
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
  // Configures the Chrome app in:
  //   //chrome/test/data/chromeos/app_mode/apps_and_extensions/offline_enabled_kiosk_app
  //
  // We need an offline enabled Chrome app because some tests will try to launch
  // it while offline.
  static constexpr char kChromeAppId[] = "iiigpodgfihagabpagjehoocpakbnclp";

  // TODO(crbug.com/379633748): Add IWA.
  return {KioskMixin::Config{/*name=*/"WebApp",
                             /*auto_launch_account_id=*/{},
                             {KioskMixin::SimpleWebAppOption()}},
          KioskMixin::Config{
              /*name=*/"ChromeApp",
              /*auto_launch_account_id=*/{},
              {KioskMixin::CwsChromeAppOption{
                  /*account_id=*/"offline-enabled-chrome-app@localhost",
                  /*app_id=*/kChromeAppId,
                  /*crx_filename=*/base::StrCat({kChromeAppId, ".crx"}),
                  /*crx_version=*/"2.0.0"}}}};
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
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskApp()));

  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/false);

  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(SplashScreenTest, NetworkShortcutWorksOnline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitSplashScreen();

  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  scoped_launch_blocker.reset();
  ClickNetworkScreenContinueButton();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SplashScreenTest,
                         testing::ValuesIn(SplashScreenTestConfigs()),
                         KioskMixin::ConfigName);

using OfflineLaunchEnabledSplashScreenTest = SplashScreenTest;

IN_PROC_BROWSER_TEST_P(OfflineLaunchEnabledSplashScreenTest,
                       PRE_NetworkShortcutWorksOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(OfflineLaunchEnabledSplashScreenTest,
                       NetworkShortcutWorksOffline) {
  network_state_.SimulateOffline();
  DisableGaiaOfflineScreen();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitSplashScreen();
  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  scoped_launch_blocker.reset();
  ClickNetworkScreenContinueButton();

  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OfflineLaunchEnabledSplashScreenTest,
    testing::ValuesIn(OfflineLaunchSplashScreenTestConfigs()),
    KioskMixin::ConfigName);

}  // namespace ash
