// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager_factory.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

class ShortcutCustomizationAppIntegrationTest
    : public ash::SystemWebAppIntegrationTest {
 public:
  ShortcutCustomizationAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kShortcutCustomizationApp,
         ash::features::kSearchInShortcutsApp},
        {});
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Shortcut Customization App installs and launches correctly by
// running some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(ShortcutCustomizationAppIntegrationTest,
                       ShortcutCustomizationAppInLauncher) {
  const GURL url(ash::kChromeUIShortcutCustomizationAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION,
                              url, "Shortcut Customization"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.ShortcutCustomization",
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

IN_PROC_BROWSER_TEST_P(ShortcutCustomizationAppIntegrationTest,
                       LaunchMetricsTest) {
  WaitForTestSystemAppInstall();

  ash::LaunchSystemWebAppAsync(profile(),
                               ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION);

  histogram_tester_.ExpectUniqueSample(
      "Apps.DefaultAppLaunch.FromChromeInternal", 44, 1);
}

IN_PROC_BROWSER_TEST_P(ShortcutCustomizationAppIntegrationTest,
                       ShortcutsAppManager) {
  WaitForTestSystemAppInstall();

  ash::LaunchSystemWebAppAsync(profile(),
                               ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION);

  base::RunLoop run_loop;
  // Test that the ShortcutsAppManagerFactory can retrieve the
  // ShortcutsAppManager, and that we can retrieve the SearchHandler and perform
  // a search.
  ash::shortcut_ui::ShortcutsAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/2u,
               base::BindLambdaForTesting(
                   [&](std::vector<
                       ash::shortcut_customization::mojom::SearchResultPtr>
                           search_results) {
                     // Assert that the number of results is equal to the
                     // max_num_results passed into the query.
                     // TODO(cambickel): Update this test when SearchHandler
                     // provides real data from the LocalSearchService.
                     EXPECT_EQ(search_results.size(), 2u);
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ShortcutCustomizationAppIntegrationTest);
