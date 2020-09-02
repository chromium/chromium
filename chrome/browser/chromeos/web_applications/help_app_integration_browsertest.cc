// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kHelpAppReleaseNotes}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
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

  // The metric is recorded in LaunchSystemWebApp (crbug/1112660), but using
  // AppServiceProxy gives more coverage of the launch path and ensures the
  // metric is not recorded twice.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app/"));
  navigation_observer.StartWatchingNewWebContents();

  proxy->Launch(
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::HELP),
      ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromKeyboard,
      display::kDefaultDisplayId);

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

// Test that the Help App shortcut doesn't crash an incognito browser.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2Incognito) {
  WaitForTestSystemAppInstall();

  chrome::ShowHelp(CreateIncognitoBrowser(), chrome::HELP_SOURCE_KEYBOARD);

#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/")));
#else
  // We just have 2 browsers, the incognito and regular. Navigates chrome.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            chrome::FindLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL());
#endif
}

// Test that launching the Help App's release notes opens the app on the Release
// Notes page.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2LaunchReleaseNotes) {
  WaitForTestSystemAppInstall();

  // There should be 1 browser window initially.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);
#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // The opened window should be showing the url with attached WebUI.
  content::WebContents* web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();

  // The inner frame should be the pathname for the release notes pathname.
  EXPECT_EQ("chrome-untrusted://help-app/updates",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
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
#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that launching the Help App's release notes doesn't crash an incognito
// browser.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ReleaseNotesIncognito) {
  WaitForTestSystemAppInstall();

  chrome::LaunchReleaseNotes(CreateIncognitoBrowser()->profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);

#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/updates")));
#else
  // We just have 2 browsers, the incognito and regular. No new app opens.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
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

  content::WebContents* web_contents = LaunchApp(params);

  // The inner frame should have the same pathname as the launch URL.
  EXPECT_EQ("chrome-untrusted://help-app/help/",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
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
    window.parent.postMessage('show-parental-controls', '*');
  )";
  // Trigger the postMessage, then wait for settings to open.
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  navigation_observer.Wait();

  // Settings should be active in a new window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
}

// Test that the Help App opens when Gesture help requested.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppOpenGestures) {
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

INSTANTIATE_TEST_SUITE_P(
    All,
    HelpAppIntegrationTest,
    ::testing::Combine(
        ::testing::Values(web_app::ProviderType::kBookmarkApps,
                          web_app::ProviderType::kWebApps),
        ::testing::Values(web_app::InstallationType::kManifestInstall)),
    web_app::ProviderAndInstallationTypeToString);

class HelpAppGuestSessionIntegrationTest : public HelpAppIntegrationTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Test that the Help App shortcut doesn't crash in guest mode.
IN_PROC_BROWSER_TEST_P(HelpAppGuestSessionIntegrationTest, HelpAppShowHelp) {
  WaitForTestSystemAppInstall();

  chrome::ShowHelp(browser(), chrome::HELP_SOURCE_KEYBOARD);

#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/")));
#else
  // No new app should open on non-branded builds. Navigates chrome.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            chrome::FindLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL());
#endif
}

// Test that the Help App release notes entry point doesn't crash in guest mode.
IN_PROC_BROWSER_TEST_P(HelpAppGuestSessionIntegrationTest,
                       HelpAppLaunchReleaseNotes) {
  WaitForTestSystemAppInstall();

  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);

#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/updates")));
#else
  // Nothing should happen on non-branded builds.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that Gesture help works in guest mode.
IN_PROC_BROWSER_TEST_P(HelpAppGuestSessionIntegrationTest,
                       HelpAppOpenGestures) {
  WaitForTestSystemAppInstall();

  SystemTrayClient::Get()->ShowGestureEducationHelp();

  EXPECT_NO_FATAL_FAILURE(
      WaitForAppToOpen(GURL("chrome://help-app/help/sub/3399710/id/9739838")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HelpAppGuestSessionIntegrationTest,
    ::testing::Combine(
        ::testing::Values(web_app::ProviderType::kBookmarkApps,
                          web_app::ProviderType::kWebApps),
        ::testing::Values(web_app::InstallationType::kManifestInstall)),
    web_app::ProviderAndInstallationTypeToString);
