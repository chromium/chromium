// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ShortcutCustomizationAppIntegrationTest
    : public ash::SystemWebAppIntegrationTest {
 public:
  ShortcutCustomizationAppIntegrationTest() = default;

 protected:
  base::HistogramTester histogram_tester_;
};

// Test that the Shortcut Customization App installs and launches correctly by
// running some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(ShortcutCustomizationAppIntegrationTest,
                       ShortcutCustomizationAppInLauncher) {
  const GURL url(ash::kChromeUIShortcutCustomizationAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION, url, "Key Shortcuts"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.ShortcutCustomization",
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

IN_PROC_BROWSER_TEST_P(ShortcutCustomizationAppIntegrationTest,
                       LaunchMetricsTest) {
  WaitForTestSystemAppInstall();

  const GURL url(ash::kChromeUIShortcutCustomizationAppURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  ash::LaunchSystemWebAppAsync(profile(),
                               ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION);
  observer.Wait();

  histogram_tester_.ExpectUniqueSample(
      "Apps.DefaultAppLaunch.FromChromeInternal", 44, 1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ShortcutCustomizationAppIntegrationTest);
