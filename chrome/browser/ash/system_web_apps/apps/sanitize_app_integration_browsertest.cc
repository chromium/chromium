// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

class SanitizeAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  SanitizeAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSanitize},
        /*disabled_featuers=*/{});
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Sanitize App installs properly.
IN_PROC_BROWSER_TEST_P(SanitizeAppIntegrationTest, SanitizeAppInLauncher) {
  const GURL url((ash::kChromeUISanitizeAppURL));
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::OS_SANITIZE, url, "Safety reset"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.Sanitize",
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

IN_PROC_BROWSER_TEST_P(SanitizeAppIntegrationTest, LaunchMetricsTest) {
  WaitForTestSystemAppInstall();

  const GURL url((ash::kChromeUISanitizeAppURL));
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::OS_SANITIZE);
  observer.Wait();

  histogram_tester_.ExpectUniqueSample(
      "Apps.DefaultAppLaunch.FromChromeInternal", 60, 1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SanitizeAppIntegrationTest);
