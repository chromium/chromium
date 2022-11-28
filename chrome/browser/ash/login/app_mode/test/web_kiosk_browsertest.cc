// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_screen_handler.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

using ::testing::_;

const char kAppInstallUrl[] = "https://app.com/install";
const char kAppLaunchUrl[] = "https://app.com/launch";
const char16_t kAppTitle[] = u"title.";
const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

class FakeKioskProfileLoaderDelegate : public KioskProfileLoader::Delegate {
 public:
  MOCK_METHOD1(OnProfileLoaded, void(Profile*));
  MOCK_METHOD1(OnProfileLoadFailed, void(KioskAppLaunchError::Error));
  MOCK_METHOD1(OnOldEncryptionDetected, void(std::unique_ptr<UserContext>));
};

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
    network_wait_override_ =
        KioskLaunchController::SetNetworkWaitForTesting(base::Seconds(0));
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
    int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
    policy::SetDeviceLocalAccounts(settings_->owner_settings_service(),
                                   device_local_accounts);
    // Wait for the Kiosk App configuration to reload.
    LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
  }

  void MakeAppAlreadyInstalled() {
    if (!base::FeatureList::IsEnabled(::features::kKioskEnableAppService)) {
      WebAppInstallInfo info;
      info.start_url = GURL(kAppLaunchUrl);
      info.title = kAppTitle;
      WebKioskAppManager::Get()->UpdateAppByAccountId(account_id(), info);
      return;
    }

    // Intercept URL loader to avoid installing a placeholder app.
    content::URLLoaderInterceptor url_interceptor(base::BindRepeating(
        [](content::URLLoaderInterceptor::RequestParams* params) {
          content::URLLoaderInterceptor::WriteResponse(
              "content/test/data/simple_page.html", params->client.get());
          return true;
        }));

    FakeKioskProfileLoaderDelegate fake_delegate;
    KioskProfileLoader profile_loader(account_id(), KioskAppType::kWebApp,
                                      &fake_delegate);

    base::RunLoop loop;
    EXPECT_CALL(fake_delegate, OnProfileLoaded(_))
        .WillOnce([&loop](Profile* profile) {
          // When Kiosk profile is loaded, install Kiosk web app to
          // WebAppProvider.
          auto* provider =
              web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
          web_app::ExternalInstallOptions install_options(
              GURL(kAppInstallUrl), web_app::UserDisplayMode::kStandalone,
              web_app::ExternalInstallSource::kKiosk);
          install_options.install_placeholder = true;
          provider->externally_managed_app_manager().InstallNow(
              install_options,
              base::BindLambdaForTesting(
                  [&loop](const GURL& install_url,
                          web_app::ExternallyManagedAppManager::InstallResult
                              result) {
                    ASSERT_TRUE(webapps::IsSuccess(result.code));
                    Shell::Get()->session_controller()->RequestSignOut();
                    loop.Quit();
                  }));
        });

    profile_loader.Start();
    loop.Run();
  }

  bool LaunchApp() {
    return LoginScreenTestApi::LaunchApp(
        WebKioskAppManager::Get()->GetAppByAccountId(account_id())->app_id());
  }

  void InitializeRegularOnlineKiosk() {
    SetOnline(true);
    PrepareAppLaunch();
    LaunchApp();
    KioskSessionInitializedWaiter().Wait();
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
        KeyboardController::Get()->GetKeyboardConfig();

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
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
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
  InitializeRegularOnlineKiosk();
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

// App Service launcher requires installing web apps to Kiosk profile before
// launching offline.
IN_PROC_BROWSER_TEST_F(WebKioskTest, PRE_AlreadyInstalledOffline) {
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
}

// Runs the kiosk app offline when it has been already installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, AlreadyInstalledOffline) {
  SetOnline(false);
  PrepareAppLaunch();
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
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  ASSERT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ true);
  SetBlockAppLaunch(false);

  KioskSessionInitializedWaiter().Wait();
}

// App Service launcher requires installing web apps to Kiosk profile before
// launching offline.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       PRE_AlreadyInstalledWithConfigureAcceleratorPressed) {
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
}

// In case when the app was already installed, we should expect to be able to
// configure network without need to be online.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       AlreadyInstalledWithConfigureAcceleratorPressed) {
  SetOnline(false);
  PrepareAppLaunch();
  // Set the threshold to a max value to disable the offline message screen,
  // otherwise it would interfere with app launch.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->signin_screen_handler()
      ->SetOfflineTimeoutForTesting(base::TimeDelta::Max());
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();

  ASSERT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));

  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ false);

  KioskSessionInitializedWaiter().Wait();
}

// The shelf should be forcedly hidden in the web kiosk session.
IN_PROC_BROWSER_TEST_F(WebKioskTest, HiddenShelf) {
  InitializeRegularOnlineKiosk();

  // The shelf should be hidden at the beginning.
  EXPECT_FALSE(ShelfTestApi().IsVisible());

  // Simulate the swipe-up gesture.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  BrowserWindow* browser_window = BrowserList::GetInstance()->get(0)->window();
  gfx::NativeWindow window = browser_window->GetNativeWindow()->GetRootWindow();
  const gfx::Rect display_bounds = window->bounds();
  const gfx::Point start_point = gfx::Point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ShelfConfig::Get()->shelf_size() / 2);
  gfx::Point end_point(start_point.x(), start_point.y() - 80);
  ui::test::EventGenerator event_generator(window);
  event_generator.GestureScrollSequence(start_point, end_point,
                                        base::Milliseconds(500), 4);

  // The shelf should be still hidden after the gesture.
  EXPECT_FALSE(ShelfTestApi().IsVisible());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, KeyboardConfigPolicy) {
  InitializeRegularOnlineKiosk();
  ExpectKeyboardConfig();
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, OpenA11ySettings) {
  InitializeRegularOnlineKiosk();

  Browser* settings_browser =
      OpenA11ySettingsBrowser(WebKioskAppManager::Get()->app_session());

  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
}

// If only the a11y settings window remains open, it should be automatically
// closed in the web kiosk session.
IN_PROC_BROWSER_TEST_F(WebKioskTest, CloseSettingWindowIfOnlyOpen) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);

  AppSessionAsh* app_session = WebKioskAppManager::Get()->app_session();

  Browser* settings_browser = OpenA11ySettingsBrowser(app_session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close the initial browser.
  initial_browser->window()->Close();
  // Ensure |settings_browser| is closed too.
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  settings_browser_closed_waiter.WaitUntilClosed();

  // No browsers are opened in the web kiosk session, so it should be
  // terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  EXPECT_TRUE(app_session->is_shutting_down());
}

// Closing the a11y settings window should not exit the web app kiosk
// session if another browser is opened.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NotExitIfCloseSettingsWindow) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  AppSessionAsh* app_session = WebKioskAppManager::Get()->app_session();

  Browser* settings_browser = OpenA11ySettingsBrowser(app_session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close |settings_browser| and ensure it is closed.
  settings_browser->window()->Close();
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  settings_browser_closed_waiter.WaitUntilClosed();

  // The initial browsers should still be opened and so the kiosk session should
  // not be terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(app_session->is_shutting_down());
}

}  // namespace
}  // namespace ash
