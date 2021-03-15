// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace chromeos {

namespace {

const char kAppInstallUrl[] = "https://app.com/install";
const char kAppLaunchUrl[] = "https://app.com/launch";
const char kAppTitle[] = "title.";
const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

}  // namespace

class WebKioskTest : public OobeBaseTest {
 public:
  WebKioskTest()
      : account_id_(
            AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
                kAppInstallUrl,
                policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP))) {
    set_exit_when_last_browser_closes(false);
    needs_background_networking_ = true;
    skip_splash_wait_override_ =
        KioskLaunchController::SkipSplashScreenWaitForTesting();
    network_wait_override_ = KioskLaunchController::SetNetworkWaitForTesting(
        base::TimeDelta::FromSeconds(0));
  }

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  void TearDownOnMainThread() override {
    settings_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  void SetOnline(bool online) {
    network_portal_detector_.SimulateDefaultNetworkState(
        online ? NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE
               : NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  }

  const AccountId& account_id() { return account_id_; }

  void PrepareAppLaunch() {
    std::vector<policy::DeviceLocalAccount> device_local_accounts = {
        policy::DeviceLocalAccount(
            policy::WebKioskAppBasicInfo(kAppInstallUrl, "", ""),
            kAppInstallUrl)};

    settings_ = std::make_unique<ScopedDeviceSettings>();
    int ui_update_count = ash::LoginScreenTestApi::GetUiUpdateCount();
    policy::SetDeviceLocalAccounts(settings_->owner_settings_service(),
                                   device_local_accounts);
    // Wait for the Kiosk App configuration to reload.
    ash::LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
  }

  void MakeAppAlreadyInstalled() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(kAppLaunchUrl);
    info->title = base::UTF8ToUTF16(kAppTitle);
    WebKioskAppManager::Get()->UpdateAppByAccountId(account_id(),
                                                    std::move(info));
  }

  bool LaunchApp() {
    return ash::LoginScreenTestApi::LaunchApp(
        WebKioskAppManager::Get()->GetAppByAccountId(account_id())->app_id());
  }

  void SetBlockAppLaunch(bool block) {
    if (block)
      block_app_launch_override_ =
          KioskLaunchController::BlockAppLaunchForTesting();
    else
      block_app_launch_override_.reset();
  }

  void WaitNetworkConfigureScreenAndContinueWithOnlineState(
      bool require_network,
      bool auto_close = false) {
    OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
    SetOnline(false);
    // Unblock app launch after the network configure screen is shown.
    SetBlockAppLaunch(false);
    test::OobeJS().ExpectPathDisplayed(!require_network,
                                       kNetworkConfigureScreenContinueButton);
    SetOnline(true);

    if (!auto_close) {
      // Wait for update.
      // Continue button should be visible since we are online.
      test::OobeJS()
          .CreateDisplayedWaiter(true, kNetworkConfigureScreenContinueButton)
          ->Wait();
      test::OobeJS().ExpectPathDisplayed(true,
                                         kNetworkConfigureScreenContinueButton);
      // Click on continue button.
      test::OobeJS().TapOnPath(kNetworkConfigureScreenContinueButton);
    }
  }

  void ExpectKeyboardConfig() {
    const keyboard::KeyboardConfig config =
        ash::KeyboardController::Get()->GetKeyboardConfig();

    // `auto_capitalize` is not controlled by the policy
    // 'VirtualKeyboardFeatures', and its default value remains true.
    EXPECT_TRUE(config.auto_capitalize);

    // The other features are controlled by the policy
    // 'VirtualKeyboardFeatures', and their default values should be false.
    EXPECT_FALSE(config.auto_complete);
    EXPECT_FALSE(config.auto_correct);
    EXPECT_FALSE(config.handwriting);
    EXPECT_FALSE(config.spell_check);
    EXPECT_FALSE(config.voice_input);
  }

 private:
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  const AccountId account_id_;
  std::unique_ptr<ScopedDeviceSettings> settings_;

  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;
  std::unique_ptr<base::AutoReset<base::TimeDelta>> network_wait_override_;
  std::unique_ptr<base::AutoReset<bool>> block_app_launch_override_;
  // Web kiosks do not support consumer-based kiosk. Network can always be
  // configured.
  ScopedCanConfigureNetwork can_configure_network_override_{true, false};
};

// Runs the kiosk app when the network is always present.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowOnline) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app when the network is not present in the beginning, but
// appears later.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowBecomesOnline) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();
  SetOnline(true);
  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app without a network connection, waits till network wait
// times out. Network configure dialog appears. Afterwards, it configures
// network and closes network configure dialog. Launch proceeds.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkTimeout) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();

  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /*require_network*/ true, /*auto_close*/ true);

  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app offline when it has been already installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, AlreadyInstalledOffline) {
  SetOnline(false);
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

// Presses a network configure dialog accelerator during app launch which will
// interrupt the startup. We expect this dialog not to require network since the
// app have not yet been installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, LaunchWithConfigureAcceleratorPressed) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  test::ExecuteOobeJS(
      "cr.ui.Oobe.handleAccelerator(\"app_launch_network_config\")");
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ true);
  SetBlockAppLaunch(false);

  KioskSessionInitializedWaiter().Wait();
}

// In case when the app was already installed, we should expect to be able to
// configure network without need to be online.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       AlreadyInstalledWithConfigureAcceleratorPressed) {
  SetOnline(false);
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  test::ExecuteOobeJS(
      "cr.ui.Oobe.handleAccelerator(\"app_launch_network_config\")");
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ false);

  KioskSessionInitializedWaiter().Wait();
}

// The shelf should be forcedly hidden in the web kiosk session.
IN_PROC_BROWSER_TEST_F(WebKioskTest, HiddenShelf) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();

  // The shelf should be hidden at the beginning.
  EXPECT_FALSE(ash::ShelfTestApi().IsVisible());

  // Simulate the swipe-up gesture.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1U);
  BrowserWindow* browser_window = BrowserList::GetInstance()->get(0)->window();
  gfx::NativeWindow window = browser_window->GetNativeWindow()->GetRootWindow();
  const gfx::Rect display_bounds = window->bounds();
  const gfx::Point start_point = gfx::Point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ash::ShelfConfig::Get()->shelf_size() / 2);
  gfx::Point end_point(start_point.x(), start_point.y() - 80);
  ui::test::EventGenerator event_generator(window);
  event_generator.GestureScrollSequence(
      start_point, end_point, base::TimeDelta::FromMilliseconds(500), 4);

  // The shelf should be still hidden after the gesture.
  EXPECT_FALSE(ash::ShelfTestApi().IsVisible());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, KeyboardConfigPolicy) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();

  ExpectKeyboardConfig();
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, OpenA11ySettings) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();

  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  settings_manager->ShowOSSettings(
      profile, chromeos::settings::mojom::kManageAccessibilitySubpagePath);

  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  ASSERT_TRUE(settings_browser);
}

}  // namespace chromeos
