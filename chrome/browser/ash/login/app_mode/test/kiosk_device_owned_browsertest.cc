// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "base/barrier_closure.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/components/native_app_window/native_app_window_views.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

const test::UIPath kErrorMessageContinueButton = {"error-message",
                                                  "continueButton"};

// An app to test local access to file systems via the
// chrome.fileSystem.requestFileSystem API.
// Webstore data json is in
//     chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//         detail/enelnimkndkcejhjnpaofdlbbfmdnagi
const char kTestGetVolumeListKioskApp[] = "enelnimkndkcejhjnpaofdlbbfmdnagi";

constexpr char kSettingsPage1[] = "chrome://os-settings/manageAccessibility";
constexpr char kSettingsPage2[] =
    "chrome-extension://mndnfokpggljbaajbnioimlmbfngpief/chromevox/options/"
    "options.html";

NavigateParams OpenBrowserWithUrl(
    const std::string& url,
    WindowOpenDisposition window_type = WindowOpenDisposition::CURRENT_TAB) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  NavigateParams params(profile, GURL(url), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = window_type;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  return params;
}

// Helper class to replace settings urls for KioskSettingsNavigationThrottle.
class ScopedSettingsPages {
 public:
  explicit ScopedSettingsPages(
      std::vector<chromeos::KioskSettingsNavigationThrottle::SettingsPage>*
          pages) {
    chromeos::KioskSettingsNavigationThrottle::SetSettingPagesForTesting(pages);
  }

  ~ScopedSettingsPages() {
    chromeos::KioskSettingsNavigationThrottle::SetSettingPagesForTesting(
        nullptr);
  }
};

// Helper class to count how many times an extension has been loaded.
class ExtensionReadyObserver : public extensions::ExtensionRegistryObserver {
 public:
  ExtensionReadyObserver(extensions::ExtensionRegistry* registry,
                         const std::string& extension_id)
      : extension_id_(extension_id) {
    extension_registry_observation_.Observe(registry);
  }

  int fired_times() const { return count_; }

 private:
  // extensions::ExtensionReadyObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override {
    if (extension->id() == extension_id_)
      count_++;
  }

  int count_ = 0;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      extension_registry_observation_{this};
  const std::string extension_id_;
};

}  // namespace

// Kiosk tests with a fake device owner setup.
class KioskDeviceOwnedTest : public KioskBaseTest {
 public:
  KioskDeviceOwnedTest() {
    settings_helper_.Set(kDeviceOwner,
                         base::Value(test_owner_account_id_.GetUserEmail()));
    login_manager_.AppendRegularUsers(1);
  }

 protected:
  LoginManagerMixin login_manager_{
      &mixin_host_,
      {{LoginManagerMixin::TestUserInfo{test_owner_account_id_}}}};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, InstallAndLaunchApp) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchSuccess();
  KioskAppManager::App app;
  ASSERT_TRUE(KioskAppManager::Get()->GetApp(test_app_id(), &app));
  EXPECT_FALSE(app.was_auto_launched_with_zero_delay);
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

// This test case is to cover crbug.com/1235334.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, WindowViewsBounds) {
  ExtensionTestMessageListener app_window_loaded_listener("appWindowLoaded");

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_TRUE(app_window_loaded_listener.WaitUntilSatisfied());

  // Verify the primary user profile is existing.
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);

  // Verify the app window and views.
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* window =
      apps::AppWindowWaiter(app_window_registry, test_app_id()).Wait();
  ASSERT_TRUE(window);
  native_app_window::NativeAppWindowViews* views =
      static_cast<native_app_window::NativeAppWindowViews*>(
          window->GetBaseWindow());
  ASSERT_TRUE(views);

  // The bounds of `frame_view` and `client_view` should be consistent when the
  // Chrome app Kiosk session starts.
  views::NonClientView* non_client_view = views->widget()->non_client_view();
  const gfx::Rect& frame_view_bounds = non_client_view->frame_view()->bounds();
  const gfx::Rect& client_view_bounds =
      non_client_view->client_view()->bounds();
  EXPECT_EQ(frame_view_bounds, client_view_bounds);
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       VirtualKeyboardFeaturesEnabledByDefault) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchSuccess();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsKioskApp());

  keyboard::KeyboardConfig config =
      KeyboardController::Get()->GetKeyboardConfig();
  EXPECT_TRUE(config.auto_capitalize);
  EXPECT_TRUE(config.auto_complete);
  EXPECT_TRUE(config.auto_correct);
  EXPECT_TRUE(config.handwriting);
  EXPECT_TRUE(config.spell_check);
  EXPECT_TRUE(config.voice_input);
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, HiddenShelf) {
  ExtensionTestMessageListener app_window_loaded_listener("appWindowLoaded");
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_TRUE(app_window_loaded_listener.WaitUntilSatisfied());

  // The shelf should be hidden at the beginning.
  EXPECT_FALSE(ShelfTestApi().IsVisible());

  // Simulate the swipe-up gesture.
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);

  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* app_window =
      apps::AppWindowWaiter(app_window_registry, test_app_id()).Wait();
  ASSERT_TRUE(app_window);

  gfx::NativeWindow window = app_window->GetNativeWindow()->GetRootWindow();
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

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, ZoomSupport) {
  ExtensionTestMessageListener app_window_loaded_listener("appWindowLoaded");
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_TRUE(app_window_loaded_listener.WaitUntilSatisfied());

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);

  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* window =
      apps::AppWindowWaiter(app_window_registry, test_app_id()).Wait();
  ASSERT_TRUE(window);

  test::JSChecker window_js(window->web_contents());
  int original_width = window_js.GetInt("window.innerWidth");

  content::DOMMessageQueue message_queue(window->web_contents());

  // Inject window size observer that should notify this test when the app
  // window size changes during zoom operations.
  window_js.Evaluate(
      base::StringPrintf("window.addEventListener('resize', function() {"
                         "  window.domAutomationController.send({"
                         "      'name': '%s',"
                         "      'data': window.innerWidth"
                         "  });"
                         "});",
                         kSizeChangedMessage));

  native_app_window::NativeAppWindowViews* native_app_window_views =
      static_cast<native_app_window::NativeAppWindowViews*>(
          window->GetBaseWindow());
  ui::AcceleratorTarget* accelerator_target =
      static_cast<ui::AcceleratorTarget*>(native_app_window_views);

  // Zoom in. Text is bigger and content window width becomes smaller.
  accelerator_target->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_ADD, ui::EF_CONTROL_DOWN));

  const int width_zoomed_in =
      WaitForWidthChange(&message_queue, original_width);
  ASSERT_LT(width_zoomed_in, original_width);

  // Go back to normal. Window width is restored.
  accelerator_target->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_0, ui::EF_CONTROL_DOWN));

  const int width_zoom_normal =
      WaitForWidthChange(&message_queue, width_zoomed_in);
  ASSERT_EQ(width_zoom_normal, original_width);

  // Zoom out. Text is smaller and content window width becomes larger.
  accelerator_target->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN));

  const int width_zoomed_out =
      WaitForWidthChange(&message_queue, width_zoom_normal);
  ASSERT_GT(width_zoomed_out, original_width);

  // Terminate the app.
  window->GetBaseWindow()->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, NotSignedInWithGAIAAccount) {
  // Tests that the kiosk session is not considered to be logged in with a GAIA
  // account.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchSuccess();
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(app_profile)
                   ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, PRE_LaunchAppNetworkDown) {
  // Tests the network down case for the initial app download and launch.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, LaunchAppNetworkDown) {
  // Tests the network down case for launching an existing app that is
  // installed in PRE_LaunchAppNetworkDown.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       LaunchAppWithNetworkConfigAccelerator) {
  ScopedCanConfigureNetwork can_configure_network(true, false);

  // Block app loading until the welcome screen is shown.
  BlockAppLaunch(true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  OobeScreenWaiter splash_waiter(AppLaunchSplashScreenView::kScreenId);
  splash_waiter.Wait();

  // A network error screen should be shown after authenticating.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  // Simulate Ctrl+Alt+N accelerator.

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchNetworkConfig);
  error_screen_waiter.Wait();
  ASSERT_TRUE(GetKioskLaunchController()->showing_network_dialog());

  // Continue button should be visible since we are online.
  test::OobeJS().ExpectVisiblePath(kErrorMessageContinueButton);

  // Let app launching resume.
  BlockAppLaunch(false);

  // Click on [Continue] button.
  test::OobeJS().TapOnPath(kErrorMessageContinueButton);

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       LaunchAppNetworkDownConfigureNotAllowed) {
  // Mock network could not be configured.
  ScopedCanConfigureNetwork can_configure_network(false, true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  OobeScreenWaiter splash_waiter(AppLaunchSplashScreenView::kScreenId);
  splash_waiter.Wait();
  WaitForAppLaunchNetworkTimeout();

  // Configure network link should not be visible.
  test::OobeJS().ExpectHiddenPath(kConfigNetwork);

  // Network becomes online and app launch is resumed.
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, LaunchAppNetworkPortal) {
  // Mock network could be configured without the owner password.
  ScopedCanConfigureNetwork can_configure_network(true, false);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  OobeScreenWaiter app_splash_waiter(AppLaunchSplashScreenView::kScreenId);
  app_splash_waiter.set_no_assert_last_screen();
  app_splash_waiter.Wait();

  WaitForAppLaunchNetworkTimeout();

  // Network error should show up automatically since this test does not
  // require owner auth to configure network.
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  ASSERT_TRUE(GetKioskLaunchController()->showing_network_dialog());
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

// TODO(b/231213239): Flaky seg faults
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, DISABLED_LaunchAppUserCancel) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  OobeScreenWaiter splash_waiter(AppLaunchSplashScreenView::kScreenId);
  // Do not let the app be run to avoid race condition.
  BlockAppLaunch(true);
  splash_waiter.Wait();

  base::RunLoop run_loop;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(run_loop.QuitClosure());
  settings_helper_.SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, true);
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchBailout);
  run_loop.Run();
  EXPECT_EQ(KioskAppLaunchError::Error::kUserCancel,
            KioskAppLaunchError::Get());
}

// Verifies available volumes for kiosk apps in kiosk session.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, GetVolumeList) {
  set_test_app_id(kTestGetVolumeListKioskApp);
  set_test_app_version("0.1");
  set_test_crx_file(test_app_id() + ".crx");

  extensions::ResultCatcher catcher;
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, OpenA11ySettings) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  Browser* settings_browser =
      OpenA11ySettingsBrowser(KioskAppManager::Get()->app_session());
  ASSERT_TRUE(settings_browser);
}

IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, SettingsWindow) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  // At this moment, app session should be initialized.
  std::vector<chromeos::KioskSettingsNavigationThrottle::SettingsPage>
      settings_pages = {
          {"https://page1.com/", /*allow_subpages*/ true},
          {"https://page2.com/", /*allow_subpages*/ false},
      };

  const GURL page1("https://page1.com/");
  const GURL page1_sub("https://page1.com/sub");
  const GURL page2("https://page2.com/");
  const GURL page2_sub("https://page2.com/sub");
  const GURL page3("https://page3.com/");

  // Replace the settings allowlist with `settings_pages`.
  ScopedSettingsPages pages(&settings_pages);
  AppSessionAsh* app_session = KioskAppManager::Get()->app_session();

  // App session should be initialized.
  ASSERT_TRUE(app_session);
  ASSERT_FALSE(app_session->GetSettingsBrowserForTesting());

  OpenBrowserWithUrl(page1.spec(), WindowOpenDisposition::NEW_POPUP);
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));

  Browser* settings_browser = app_session->GetSettingsBrowserForTesting();
  ASSERT_TRUE(settings_browser);

  content::WebContents* web_contents =
      settings_browser->tab_strip_model()->GetActiveWebContents();
  // Try navigating to an allowed subpage.
  NavigateToURLBlockUntilNavigationsComplete(web_contents, page1_sub, 1);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), page1_sub);

  // Open another browser with url page2.
  // Also, expect navigation inside of the old window to page2.
  content::TestNavigationObserver settings_navigation_observer(web_contents, 1);
  OpenBrowserWithUrl(page2.spec(), WindowOpenDisposition::NEW_POPUP);
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));
  // Also wait for navigaiton to finish.
  settings_navigation_observer.Wait();

  // The settings browser should not have changed.
  ASSERT_EQ(settings_browser, app_session->GetSettingsBrowserForTesting());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), page2);

  // Try navigating to a disallowed subpage (this won't commit the navigation).
  NavigateToURLBlockUntilNavigationsComplete(
      web_contents, page2_sub, 1,
      /*ignore_uncommitted_navigations=*/false);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), page2);

  // Try navigating to a disallowed page (this won't commit the navigation).
  NavigateToURLBlockUntilNavigationsComplete(
      web_contents, page3, 1,
      /*ignore_uncommitted_navigations=*/false);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), page2);

  // Close settings browser, expect the value to be cleared.
  CloseBrowserSynchronously(settings_browser);
  EXPECT_FALSE(app_session->GetSettingsBrowserForTesting());

  // Open another browser with url page2, but now of the default type.
  // This should create a new browser of app type, and close the non-app one.
  NavigateParams params = OpenBrowserWithUrl(page2.spec());
  // Wait for two browser handlings -- for non-app and app browser.
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));

  // One browser should be created.
  settings_browser = app_session->GetSettingsBrowserForTesting();
  ASSERT_TRUE(settings_browser);
  EXPECT_FALSE(params.browser == settings_browser);
}

// This test covers b/245088137: after opening the settings browser and moving
// focus to the main kiosk app, the settings browser could not be opened again.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, SettingsWindowShouldBeActive) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);
  AppSessionAsh* app_session = KioskAppManager::Get()->app_session();

  // App session should be initialized.
  ASSERT_TRUE(app_session);
  ASSERT_FALSE(app_session->GetSettingsBrowserForTesting());

  OpenBrowserWithUrl(kSettingsPage1, WindowOpenDisposition::NEW_POPUP);
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));

  Browser* settings_browser = app_session->GetSettingsBrowserForTesting();

  // Make sure the settings browser was opened, and it is focused.
  ASSERT_TRUE(settings_browser);
  EXPECT_TRUE(settings_browser->window()->IsActive());

  // Emulate focus switching.
  settings_browser->window()->Deactivate();
  EXPECT_FALSE(settings_browser->window()->IsActive());

  content::WebContents* web_contents =
      settings_browser->tab_strip_model()->GetActiveWebContents();

  // Open another settings browser.
  // Also, expect navigation inside of the old window to kSettingsPage2.
  content::TestNavigationObserver settings_navigation_observer(web_contents, 1);
  OpenBrowserWithUrl(kSettingsPage2, WindowOpenDisposition::NEW_POPUP);
  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));
  // Also wait for navigaiton to finish.
  settings_navigation_observer.Wait();

  // The settings browser should not have changed.
  ASSERT_EQ(settings_browser, app_session->GetSettingsBrowserForTesting());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GURL(kSettingsPage2));

  // The settings browser should be focused again.
  EXPECT_TRUE(settings_browser->window()->IsActive());
}

// If only the a11y settings window remains open, it should not be automatically
// closed in the chrome app kiosk session.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, SettingsWindowRemainsOpen) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);
  AppSessionAsh* app_session = KioskAppManager::Get()->app_session();
  // App session should be initialized.
  ASSERT_NE(app_session, nullptr);

  OpenA11ySettingsBrowser(app_session);
  Browser* settings_browser = app_session->GetSettingsBrowserForTesting();
  ASSERT_NE(settings_browser, nullptr);

  // Only the settings browser is opened.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(app_session->is_shutting_down());
}

// Closing the a11y settings window should not exit the chrome app kiosk
// session.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest, CloseSettingsWindow) {
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);
  AppSessionAsh* app_session = KioskAppManager::Get()->app_session();
  // App session should be initialized.
  ASSERT_NE(app_session, nullptr);

  OpenA11ySettingsBrowser(app_session);
  Browser* settings_browser = app_session->GetSettingsBrowserForTesting();
  ASSERT_NE(settings_browser, nullptr);
  ASSERT_NE(settings_browser->window(), nullptr);

  // Only the settings browser is opened.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(app_session->is_shutting_down());

  settings_browser->window()->Close();
  // Ensure |settings_browser| is closed.
  TestBrowserClosedWaiter browser_closed_waiter{settings_browser};
  browser_closed_waiter.WaitUntilClosed();

  // No browsers are opened in the chrome app kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  EXPECT_FALSE(app_session->is_shutting_down());
}

// Verifies that an enterprise device does not auto-launch kiosk mode when cros
// settings are untrusted.
IN_PROC_BROWSER_TEST_F(KioskDeviceOwnedTest,
                       NoEnterpriseAutoLaunchWhenUntrusted) {
  PrepareAppLaunch();
  SimulateNetworkOnline();

  // Make cros settings untrusted.
  settings_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);

  // Trigger the code that handles auto-launch on enterprise devices. This would
  // normally be called from ShowLoginWizard(), which runs so early that it is
  // not possible to inject an auto-launch policy before it runs.
  LoginDisplayHost* login_display_host = LoginDisplayHost::default_host();
  ASSERT_TRUE(login_display_host);
  login_display_host->StartKiosk(KioskAppId::ForChromeApp(test_app_id()), true);

  // Check that no launch has started.
  EXPECT_FALSE(login_display_host->GetKioskLaunchController());
}

// This test verifies that accessibility extensions do not preserve any local
// data in-between session, as opposed to what they usually do in user sessions.
// See crbug.com/1049566
IN_PROC_BROWSER_TEST_F(
    KioskDeviceOwnedTest,
    PRE_AccessibilityExtensionsResetTheirStateUponSessionRestart) {
  test::SpeechMonitor speech_monitor;
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();

  ExtensionReadyObserver ready_observer(
      extensions::ExtensionRegistry::Get(app_profile),
      extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor.Replay();

  // Extension should be loaded only once.
  EXPECT_EQ(ready_observer.fired_times(), 1);

  constexpr char kSetInStorageAPI[] =
      R"(chrome.storage.local.set(
             {test: 'testValue'},
             () => { domAutomationController.send('') });)";
  // Store some data using Storage API for the extension.
  extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      app_profile, extension_misc::kChromeVoxExtensionId, kSetInStorageAPI);

  // Expect the data to be saved.
  constexpr char kGetFromStorageAPI[] =
      R"(chrome.storage.local.get(
             'test',
             (value) => domAutomationController.send(value.test));)";
  EXPECT_EQ("testValue",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));

  // The data should persist when extension is restarted.
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  test::SpeechMonitor speech_monitor2;
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor2.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor2.Replay();

  // Expect the data to be still there.
  EXPECT_EQ("testValue",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));
}

// TODO(crbug.com/1395435): Re-enable this test
IN_PROC_BROWSER_TEST_F(
    KioskDeviceOwnedTest,
    DISABLED_AccessibilityExtensionsResetTheirStateUponSessionRestart) {
  test::SpeechMonitor speech_monitor;
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();

  ExtensionReadyObserver ready_observer(
      extensions::ExtensionRegistry::Get(app_profile),
      extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor.Replay();

  // Extension should be loaded only once.
  EXPECT_EQ(ready_observer.fired_times(), 1);

  // Expect the data to be cleared.
  constexpr char kGetFromStorageAPI[] =
      R"(
      chrome.storage.local.get(
          "test",
          function(value) {
              domAutomationController.send(value.test == undefined ?
                  "<none>" : value.test);
          }
      );
      )";
  EXPECT_EQ("<none>",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromStorageAPI));

  constexpr char kGetFromLocalStorage[] =
      R"(
      domAutomationController.send(
          localStorage.getItem('test2') == undefined ?
              "<none>" : localStorage.getItem('test2'));
      )";
  EXPECT_EQ("<none>",
            extensions::browsertest_util::ExecuteScriptInBackgroundPage(
                app_profile, extension_misc::kChromeVoxExtensionId,
                kGetFromLocalStorage));
}

}  // namespace ash
