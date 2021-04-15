// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/chromeos/release_notes/release_notes_notification.h"
#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kReleaseNotesNotificationAllChannels}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using HelpAppAllProfilesIntegrationTest = HelpAppIntegrationTest;

content::WebContents* GetActiveWebContents() {
  return chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
}

// Waits for and expects that the correct url is opened.
void WaitForAppToOpen(const GURL& expected_url) {
  // Start with a number of browsers (may include an incognito browser).
  size_t num_browsers = chrome::GetTotalBrowserCount();
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be another browser window for the newly opened app.
  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  // Help app should have opened at the expected page.
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(chromeos::kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::HELP, url, "Explore"));
}

// Test that the Help App is searchable by additional strings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2SearchInLauncher) {
  WaitForTestSystemAppInstall();
  EXPECT_EQ(
      std::vector<std::string>({"Get Help", "Perks", "Offers"}),
      GetManager().GetAdditionalSearchTerms(web_app::SystemAppType::HELP));
}

// Test that the Help App has a minimum window size of 600x320.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2MinWindowSize) {
  WaitForTestSystemAppInstall();
  auto app_id = LaunchParamsForApp(web_app::SystemAppType::HELP).app_id;
  EXPECT_EQ(GetManager().GetMinimumWindowSize(app_id), gfx::Size(600, 320));
}

// Test that the Help App has a default size of 960x600 and is in the center of
// the screen.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DefaultWindowBounds) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(web_app::SystemAppType::HELP, &browser);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  int x = (work_area.width() - 960) / 2;
  int y = (work_area.height() - 600) / 2;
  EXPECT_EQ(browser->window()->GetBounds(), gfx::Rect(x, y, 960, 600));
}

// Test that the Help App logs metric when launching the app using the
// AppServiceProxy.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2AppServiceMetrics) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Using AppServiceProxy gives more coverage of the launch path and ensures
  // the metric is not recorded twice.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app/"));
  navigation_observer.StartWatchingNewWebContents();

  proxy->Launch(
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::HELP),
      ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromKeyboard,
      apps::MakeWindowInfo(display::kDefaultDisplayId));

  navigation_observer.Wait();
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      1);
}

// Test that the Help App can log metrics in the untrusted frame.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2InAppMetrics) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  base::UserActionTester user_action_tester;

  constexpr char kScript[] = R"(
    chrome.metricsPrivate.recordUserAction("Discover.Help.TabClicked");
  )";

  EXPECT_EQ(0, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
}

IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppV2ShowHelp) {
  WaitForTestSystemAppInstall();

  chrome::ShowHelp(browser(), chrome::HELP_SOURCE_KEYBOARD);

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/")));
#else
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
#endif
}

// Test that launching the Help App's release notes opens the app on the Release
// Notes page.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppV2LaunchReleaseNotes) {
  WaitForTestSystemAppInstall();

  // There should be 1 browser window initially.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // The opened window should be showing the url with attached WebUI.
  // The inner frame should be the pathname for the release notes pathname.
  EXPECT_EQ("chrome-untrusted://help-app/updates",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                GetActiveWebContents(), "window.location.href"));
#else
  // Nothing should happen on non-branded builds.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that launching the Help App's release notes logs metrics.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ReleaseNotesMetrics) {
  WaitForTestSystemAppInstall();

  base::UserActionTester user_action_tester;
  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that clicking the release notes notification opens Help App.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2LaunchReleaseNotesFromNotification) {
  WaitForTestSystemAppInstall();
  base::UserActionTester user_action_tester;
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  auto release_notes_notification =
      std::make_unique<chromeos::ReleaseNotesNotification>(profile());
  auto release_notes_storage =
      std::make_unique<chromeos::ReleaseNotesStorage>(profile());

  // Force the release notes notification to show up.
  profile()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone, 20);
  release_notes_notification->MaybeShowReleaseNotes();
  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ("show_release_notes_notification", notifications[0].id());
  // Then click.
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 "show_release_notes_notification",
                                 base::nullopt, base::nullopt);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("ReleaseNotes.NotificationShown"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ReleaseNotes.LaunchedNotification"));
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/updates")));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that the Help App does a navigation on launch even when it was already
// open with the same URL.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2NavigateOnRelaunch) {
  WaitForTestSystemAppInstall();

  // There should initially be a single browser window.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(web_app::SystemAppType::HELP, &browser);

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(web_contents);
  LaunchAppWithoutWaiting(web_app::SystemAppType::HELP);
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // LaunchApp should navigate the existing window and not open any new windows.
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

// Test direct navigation to a subpage.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DirectNavigation) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::HELP);
  params.override_url = GURL("chrome://help-app/help/");

  content::WebContents* web_contents = LaunchApp(std::move(params));

  // The inner frame should have the same pathname as the launch URL.
  EXPECT_EQ("chrome-untrusted://help-app/help/",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
}

// Test that the Help App can open the feedback dialog.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2OpenFeedbackDialog) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // Script that tells the Help App to open the feedback dialog.
  constexpr char kScript[] = R"(
    (async () => {
      const app = document.querySelector('showoff-app');
      const res = await app.getDelegate().openFeedbackDialog();
      window.domAutomationController.send(res === null);
    })();
  )";
  bool error_is_null;
  // Use ExecuteScript instead of EvalJsInAppFrame because the script needs to
  // run in the same world as the page's code.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &error_is_null));
  // A null string result means no error in opening feedback.
  EXPECT_TRUE(error_is_null);
}

// Test that the Help App opens the OS Settings family link page.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ShowParentalControls) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://os-settings/osPeople");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  // Script that tells the Help App to show parental controls.
  constexpr char kScript[] = R"(
    (async () => {
      const app = document.querySelector('showoff-app');
      await app.getDelegate().showParentalControls();
    })();
  )";
  // Trigger the script, then wait for settings to open. Use ExecuteScript
  // instead of EvalJsInAppFrame because the script needs to run in the same
  // world as the page's code.
  EXPECT_TRUE(content::ExecuteScript(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  navigation_observer.Wait();

  // Settings should be active in a new window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App opens when Gesture help requested.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppOpenGestures) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  SystemTrayClient::Get()->ShowGestureEducationHelp();

  EXPECT_NO_FATAL_FAILURE(
      WaitForAppToOpen(GURL("chrome://help-app/help/sub/3399710/id/9739838")));
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromOtherApp", 18,
                                      1);
}

// Test that the Help App opens from keyboard shortcut.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppOpenKeyboardShortcut) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // The /? key is OEM_2 on a US standard keyboard.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_OEM_2, /*control=*/true,
      /*shift=*/false, /*alt=*/false, /*command=*/false));

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Default browser tab and Help app are open.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ("chrome://help-app/", GetActiveWebContents()->GetVisibleURL());
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      1);
#else
  // We just have the one browser. Navigates chrome.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      0);
#endif
}

// Test that the Help App opens in a new window if try to navigate there in a
// browser.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppCapturesBrowserNavigation) {
  WaitForTestSystemAppInstall();
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app"));
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Try to navigate to the help app in the browser.
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "chrome://help-app");
  navigation_observer.Wait();

  // We now have two browsers, one for the chrome window, one for the Help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL("chrome://help-app"), GetActiveWebContents()->GetVisibleURL());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    HelpAppAllProfilesIntegrationTest);
