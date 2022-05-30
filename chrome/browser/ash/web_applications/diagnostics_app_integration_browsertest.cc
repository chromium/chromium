// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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
// Same as feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess enum.
const size_t kUsedWithSuccess = 2;
}  // namespace

class DiagnosticsAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  DiagnosticsAppIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kEnableNetworkingInDiagnosticsApp);
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

  web_app::LaunchSystemWebAppAsync(profile(),
                                   ash::SystemWebAppType::DIAGNOSTICS);
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  histogram_tester_.ExpectUniqueSample(kFromChromeLaunch, kDiagnosticsApp, 1);
}

IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest, UsageMetricsTest) {
  WaitForTestSystemAppInstall();

  Browser* system_app_browser;
  // Launch app and allow UI to load.
  LaunchApp(ash::SystemWebAppType::DIAGNOSTICS, &system_app_browser);

  // Find system browser for diagnostics and close it to trigger usage metrics.
  EXPECT_TRUE(web_app::IsSystemWebApp(system_app_browser));
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

  EXPECT_TRUE(content::ExecuteScript(
      web_contents, "chrome.send('recordNavigation', [0, 1]);"));
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  chrome::CloseAllBrowsers();
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

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
  EXPECT_TRUE(content::ExecuteScript(
      web_contents, "chrome.send('recordNavigation', [1000, -550]);"));
  EXPECT_TRUE(content::ExecuteScript(
      web_contents, "chrome.send('recordNavigation', ['1000', '-550']);"));
  EXPECT_TRUE(
      content::ExecuteScript(web_contents, "chrome.send('recordNavigation');"));
  EXPECT_TRUE(content::ExecuteScript(web_contents,
                                     "chrome.send('recordNavigation', []);"));
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  chrome::CloseAllBrowsers();
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.System.OpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.Connectivity.OpenDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.DiagnosticsUi.Input.OpenDuration", 0);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    DiagnosticsAppIntegrationTest);
