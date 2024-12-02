// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string_view>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/test/gtest_tags.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

Profile& CurrentProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

bool IsWebAppInstalled(const KioskApp& app) {
  auto& profile = CurrentProfile();
  auto [state, __] =
      chromeos::GetKioskWebAppInstallState(profile, app.url().value());
  return crosapi::mojom::WebKioskInstallState::kInstalled == state;
}

Browser::CreateParams CreateNewBrowserParams(Browser* initial_kiosk_browser,
                                             bool is_popup_browser) {
  return is_popup_browser
             ? Browser::CreateParams::CreateForAppPopup(
                   initial_kiosk_browser->app_name(),
                   /*trusted_source=*/true,
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

void WaitNetworkScreen() {
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
}

void WaitSplashScreen() {
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
}

bool PressNetworkAccelerator() {
  return LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

void ExpectNetworkScreenContinueButtonShown(bool is_shown) {
  test::OobeJS().ExpectPathDisplayed(is_shown,
                                     kNetworkConfigureScreenContinueButton);
}

void ClickNetworkScreenContinueButton() {
  test::OobeJS().ClickOnPath(kNetworkConfigureScreenContinueButton);
}

[[nodiscard]] std::optional<base::AutoReset<bool>> BlockKioskLaunch() {
  return {KioskTestHelper::BlockAppLaunch()};
}

// Returns the web app configured in Kiosk.
KioskApp TheKioskWebApp() {
  auto apps = KioskController::Get().GetApps();
  CHECK_EQ(apps.size(), 1ul);
  CHECK_EQ(apps[0].id().type, KioskAppType::kWebApp);
  return apps[0];
}

}  // namespace

// Verifies general Kiosk features that only apply to web apps.
class WebKioskTest : public MixinBasedInProcessBrowserTest {
 public:
  WebKioskTest() = default;

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  ~WebKioskTest() override = default;

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/{/*name=*/{},
                                /*auto_launch_account_id=*/{},
                                {KioskMixin::SimpleWebAppOption()}}};
};

// Runs the kiosk app without a network connection, waits till network wait
// times out. Network configure dialog appears. Afterwards, it configures
// network and closes network configure dialog. Launch proceeds.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkTimeout) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));

  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/false);

  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsWebAppInstalled(TheKioskWebApp())));
}

// Presses a network configure dialog accelerator during app launch which will
// interrupt the startup.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkShortcutWorks) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));

  // Block launch so the test has time to press the network accelerator.
  auto block_launch_override = BlockKioskLaunch();
  WaitSplashScreen();
  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  block_launch_override.reset();
  ClickNetworkScreenContinueButton();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsWebAppInstalled(TheKioskWebApp())));
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, PRE_NetworkShortcutWorksOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsWebAppInstalled(TheKioskWebApp())));
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkShortcutWorksOffline) {
  network_state_.SimulateOffline();
  DisableGaiaOfflineScreen();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));

  auto block_launch_override = BlockKioskLaunch();
  WaitSplashScreen();
  ASSERT_TRUE(PressNetworkAccelerator());
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonShown(/*is_shown=*/true);

  block_launch_override.reset();
  ClickNetworkScreenContinueButton();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE((IsWebAppInstalled(TheKioskWebApp())));
}
IN_PROC_BROWSER_TEST_F(WebKioskTest, KeyboardConfigPolicy) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

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

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewPopupBrowserInKioskNotAllowedByDefault) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

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
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

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
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(&session));
  ASSERT_NE(new_popup_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  EXPECT_FALSE(initial_browser->GetBrowserView().CanUserEnterFullscreen());
  EXPECT_FALSE(new_popup_browser->GetBrowserView().CanUserEnterFullscreen());
  EXPECT_TRUE(initial_browser->GetBrowserView().IsFullscreen());
  EXPECT_TRUE(new_popup_browser->GetBrowserView().IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedEvenByPolicy) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

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
                                   public testing::WithParamInterface<bool> {
 public:
  WebKioskOfflineEnabledTest() = default;

  WebKioskOfflineEnabledTest(const WebKioskOfflineEnabledTest&) = delete;
  WebKioskOfflineEnabledTest& operator=(const WebKioskOfflineEnabledTest&) =
      delete;

  ~WebKioskOfflineEnabledTest() override = default;

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
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  ASSERT_TRUE(IsWebAppInstalled(TheKioskWebApp()));
}

IN_PROC_BROWSER_TEST_P(WebKioskOfflineEnabledTest, AlreadyInstalledOffline) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-35e430a3-04b3-46a7-aa0a-207a368b8cba");

  network_state_.SimulateOffline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));

  if (!IsAppOfflineEnabled()) {
    auto block_launch_override = BlockKioskLaunch();
    WaitNetworkScreen();
    ExpectNetworkScreenContinueButtonShown(/*is_shown=*/false);
    block_launch_override.reset();
    network_state_.SimulateOnline();
  }

  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
}

INSTANTIATE_TEST_SUITE_P(All, WebKioskOfflineEnabledTest, testing::Bool());

}  // namespace ash
