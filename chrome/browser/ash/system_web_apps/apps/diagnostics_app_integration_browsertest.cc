// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {
// Same as apps::DefaultAppName::kDiagnosticsApp enum.
const size_t kDiagnosticsApp = 42;
const char kDiagnosticsUmaFeatureFullPath[] =
    "ChromeOS.FeatureUsage.DiagnosticsUi";
const char kDiagnosticsUmaFeatureUsetimeFullPath[] =
    "ChromeOS.FeatureUsage.DiagnosticsUi.Usetime";
const char kDiagnosticsUmaOpenDurationFullPath[] =
    "ChromeOS.DiagnosticsUi.OpenDuration";
const char kFromChromeLaunch[] = "Apps.DefaultAppLaunch.FromChromeInternal";
const char kFindDiagnosticsAppScript[] =
    R"(document.querySelectorAll('diagnostics-app').length === 1)";
// Same as feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess enum.
const size_t kUsedWithSuccess = 2;
}  // namespace

class DiagnosticsAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  DiagnosticsAppIntegrationTest() = default;

 protected:
  base::HistogramTester histogram_tester_;

  content::WebContents* LaunchDiagnosticsApp(
      const std::string override_url = "") {
    WaitForTestSystemAppInstall();
    auto params = GetAppLaunchParams(override_url);

    return LaunchApp(std::move(params));
  }

  apps::AppLaunchParams GetAppLaunchParams(
      const std::string override_url = "") {
    auto params = LaunchParamsForApp(ash::SystemWebAppType::DIAGNOSTICS);

    // Override starting URL when provided.
    if (override_url != "") {
      params.override_url = GURL(override_url);
    }

    return params;
  }
};

// Test that the Diagnostics App installs and launches correctly by running some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppInLauncher) {
  const GURL url(ash::kChromeUIDiagnosticsAppUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::DIAGNOSTICS, url, "Diagnostics"));
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest, LaunchMetricsTest) {
  WaitForTestSystemAppInstall();

  const GURL url(ash::kChromeUIDiagnosticsAppUrl);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::DIAGNOSTICS);
  observer.Wait();

  histogram_tester_.ExpectUniqueSample(kFromChromeLaunch, kDiagnosticsApp, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest, UsageMetricsTest) {
  WaitForTestSystemAppInstall();

  Browser* system_app_browser;
  // Launch app and allow UI to load.
  LaunchApp(ash::SystemWebAppType::DIAGNOSTICS, &system_app_browser);

  // Find system browser for diagnostics and close it to trigger usage metrics.
  EXPECT_TRUE(ash::IsSystemWebApp(system_app_browser));
  chrome::CloseWindow(system_app_browser);
  ui_test_utils::WaitForBrowserToClose();

  histogram_tester_.ExpectBucketCount(kDiagnosticsUmaFeatureFullPath,
                                      kUsedWithSuccess, 1);
  histogram_tester_.ExpectTotalCount(kDiagnosticsUmaFeatureUsetimeFullPath, 1);
  histogram_tester_.ExpectTotalCount(kDiagnosticsUmaOpenDurationFullPath, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppRecordsInitialScreen) {
  LaunchDiagnosticsApp();

  // One maps to the "CrosDiagnosticsNavigationView" system enum.
  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       0, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppRecordsInitialScreenSystem) {
  // Launch Diagnostics at System UI.
  LaunchDiagnosticsApp("chrome://diagnostics/?system");

  // One maps to the "CrosDiagnosticsNavigationView" system enum.
  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       0, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppRecordsInitialScreenConnectivity) {
  // Launch Diagnostics at Connectivity UI.
  LaunchDiagnosticsApp("chrome://diagnostics/?connectivity");

  // One maps to the "CrosDiagnosticsNavigationView" connectivity enum.
  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       1, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppRecordsInitialScreenInput) {
  // Launch Diagnostics at Input UI.
  LaunchDiagnosticsApp("chrome://diagnostics/?input");

  // One maps to the "CrosDiagnosticsNavigationView" input enum.
  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       2, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppRecordsScreenOpenDuration) {
  content::WebContents* web_contents = LaunchDiagnosticsApp();

  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       0, 1);

  EXPECT_TRUE(content::ExecJs(web_contents,
                              "chrome.send('recordNavigation', [0, 1]);"));

  chrome::CloseAllBrowsers();

  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.System.OpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.Connectivity.OpenDuration", 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppIgnoresInvalidRecordNavigationCall) {
  content::WebContents* web_contents = LaunchDiagnosticsApp();

  histogram_tester_.ExpectUniqueSample("ChromeOS.DiagnosticsUi.InitialScreen",
                                       0, 1);

  // Simulate sending invalid navigation view value.
  EXPECT_TRUE(content::ExecJs(
      web_contents, "chrome.send('recordNavigation', [1000, -550]);"));
  EXPECT_TRUE(content::ExecJs(
      web_contents, "chrome.send('recordNavigation', ['1000', '-550']);"));
  EXPECT_TRUE(
      content::ExecJs(web_contents, "chrome.send('recordNavigation');"));
  EXPECT_TRUE(
      content::ExecJs(web_contents, "chrome.send('recordNavigation', []);"));

  chrome::CloseAllBrowsers();

  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.System.OpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.Connectivity.OpenDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.Input.OpenDuration", 0);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppCapturesNavigation) {
  auto* app_web_contents = LaunchDiagnosticsApp();

  const auto* app_browser = ash::FindSystemWebAppBrowser(
      profile(), ash::SystemWebAppType::DIAGNOSTICS);
  EXPECT_TRUE(app_browser);
  // DiagnosticsApp launched in its own browser.
  EXPECT_NE(browser(), app_browser);

  // Attempting to navigate to app URL returns to the existing window with
  // DiagnosticsSystemAppDelegate::ShouldCaptureNavigations() set to true.
  const GURL url(ash::kChromeUIDiagnosticsAppUrl);
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), url, /*number_of_navigations=*/2,
      WindowOpenDisposition::CURRENT_TAB, /*browser_test_flags=*/0);

  auto* browser_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::EvalJs(browser_web_contents, kFindDiagnosticsAppScript)
                   .ExtractBool());
  EXPECT_TRUE(content::EvalJs(app_web_contents, kFindDiagnosticsAppScript)
                  .ExtractBool());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    DiagnosticsAppIntegrationTest);
