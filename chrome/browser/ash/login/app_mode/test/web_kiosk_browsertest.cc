// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::TestFuture;
using ::testing::_;

using kiosk::LoadProfile;
using kiosk::LoadProfileResult;

const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

Profile& CurrentProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

bool IsWebAppInstalled(Profile& profile, const GURL& install_url) {
  auto [state, __] = chromeos::GetKioskWebAppInstallState(profile, install_url);
  return crosapi::mojom::WebKioskInstallState::kInstalled == state;
}

Browser::CreateParams CreateNewBrowserParams(Browser* initial_kiosk_browser,
                                             bool is_popup_browser) {
  return is_popup_browser
             ? Browser::CreateParams::CreateForAppPopup(
                   initial_kiosk_browser->app_name(), /*trusted_source=*/true,
                   /*window_bounds=*/gfx::Rect(),
                   initial_kiosk_browser->profile(),
                   /*user_gesture=*/true)
             : Browser::CreateParams(initial_kiosk_browser->profile(),
                                     /*user_gesture=*/true);
}

Browser* OpenNewBrowser(Browser* initial_kiosk_browser, bool is_popup_browser) {
  Browser::CreateParams params =
      CreateNewBrowserParams(initial_kiosk_browser, is_popup_browser);
  Browser* new_browser = Browser::Create(params);
  new_browser->window()->Show();
  return new_browser;
}

// Disables the Gaia screen offline message. Leaving this enable may interfere
// with checks done in offline Kiosk launch tests, since it influences the
// screens `WizardController` shows.
void DisableGaiaOfflineScreen() {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetHandler<GaiaScreenHandler>()
      ->set_offline_timeout_for_testing(base::TimeDelta::Max());
}

}  // namespace

class WebKioskTest : public WebKioskBaseTest {
 public:
  WebKioskTest() = default;

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    SetAppInstallUrl(server_mixin_.GetUrl("/title3.html"));
  }

  void SetBlockAppLaunch(bool block) {
    block_app_launch_override_ =
        block ? std::make_optional(KioskTestHelper::BlockAppLaunch())
              : std::nullopt;
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

 private:
  std::optional<base::AutoReset<bool>> block_app_launch_override_;

  FakeOriginTestServerMixin server_mixin_{
      &mixin_host_,
      /*origin=*/GURL("https://app.foo.com/"),
      /*path_to_be_served=*/FILE_PATH_LITERAL("chrome/test/data")};
};

// Runs the kiosk app when the network is always present.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowOnline) {
  InitializeRegularOnlineKiosk();
  ASSERT_TRUE(IsWebAppInstalled(CurrentProfile(), app_install_url()));
}

// Runs the kiosk app when the network is not present in the beginning, but
// appears later.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowBecomesOnline) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();
  SetOnline(true);
  KioskSessionInitializedWaiter().Wait();
  ASSERT_TRUE(IsWebAppInstalled(CurrentProfile(), app_install_url()));
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
  ASSERT_TRUE(IsWebAppInstalled(CurrentProfile(), app_install_url()));
}

// Presses a network configure dialog accelerator during app launch which will
// interrupt the startup. We expect this dialog not to require network since the
// app has not yet been installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkShortcutWorks) {
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

IN_PROC_BROWSER_TEST_F(WebKioskTest, PRE_NetworkShortcutWorksOffline) {
  InitializeRegularOnlineKiosk();
  ASSERT_TRUE(IsWebAppInstalled(CurrentProfile(), app_install_url()));
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkShortcutWorksOffline) {
  SetOnline(false);
  DisableGaiaOfflineScreen();
  PrepareAppLaunch();
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

IN_PROC_BROWSER_TEST_F(WebKioskTest, OpenA11ySettings) {
  InitializeRegularOnlineKiosk();

  Browser* settings_browser =
      OpenA11ySettingsBrowser(KioskController::Get().GetKioskSystemSession());

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

  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  Browser* settings_browser = OpenA11ySettingsBrowser(session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close the initial browser.
  initial_browser->window()->Close();
  // Ensure `settings_browser` is closed too.
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  ASSERT_TRUE(settings_browser_closed_waiter.WaitUntilClosed());

  // No browsers are opened in the web kiosk session, so it should be
  // terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  EXPECT_TRUE(session->is_shutting_down());
}

// Closing the a11y settings window should not exit the web app kiosk
// session if another browser is opened.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NotExitIfCloseSettingsWindow) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  Browser* settings_browser = OpenA11ySettingsBrowser(session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close `settings_browser` and ensure it is closed.
  settings_browser->window()->Close();
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  ASSERT_TRUE(settings_browser_closed_waiter.WaitUntilClosed());

  // The initial browsers should still be opened and so the kiosk session should
  // not be terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(session->is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewPopupBrowserInKioskNotAllowedByDefault) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  TestBrowserClosedWaiter browser_closed_waiter{new_popup_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedByDefault) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, NewPopupBrowserInKioskAllowedByPolicy) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(session));
  ASSERT_NE(new_popup_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedEvenByPolicy) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

class WebKioskOfflineEnabledTest : public WebKioskTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  WebKioskOfflineEnabledTest() = default;

  WebKioskOfflineEnabledTest(const WebKioskOfflineEnabledTest&) = delete;
  WebKioskOfflineEnabledTest& operator=(const WebKioskOfflineEnabledTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override {
    WebKioskTest::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(policy::key::kKioskWebAppOfflineEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(IsAppOfflineEnabled()),
               nullptr);
    provider_.UpdateChromePolicy(values);
  }

  bool IsAppOfflineEnabled() { return GetParam(); }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_P(WebKioskOfflineEnabledTest,
                       PRE_AlreadyInstalledOffline) {
  InitializeRegularOnlineKiosk();
  ASSERT_TRUE(IsWebAppInstalled(CurrentProfile(), app_install_url()));
}

IN_PROC_BROWSER_TEST_P(WebKioskOfflineEnabledTest, AlreadyInstalledOffline) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-35e430a3-04b3-46a7-aa0a-207a368b8cba");

  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();

  if (!IsAppOfflineEnabled()) {
    WaitNetworkConfigureScreenAndContinueWithOnlineState(
        /*require_network=*/true, /*auto_close=*/true);
  }
  KioskSessionInitializedWaiter().Wait();
}

INSTANTIATE_TEST_SUITE_P(All, WebKioskOfflineEnabledTest, ::testing::Bool());

}  // namespace ash
