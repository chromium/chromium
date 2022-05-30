// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class ShowFeedbackPageBrowserTest : public InProcessBrowserTest {
 public:
  ShowFeedbackPageBrowserTest() {
    scope_feature_list_.InitAndEnableFeature(ash::features::kOsFeedback);
  }
  ~ShowFeedbackPageBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scope_feature_list_;
};

// Test that when the policy of UserFeedbackAllowed is false, feedback app is
// not opened.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest, UserFeedbackDisallowed) {
  base::HistogramTester histogram_tester;
  std::string unused;
  chrome::ShowFeedbackPage(browser(), chrome::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);
  chrome::ShowFeedbackPage(browser(), chrome::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

// Test that when the policy of UserFeedbackAllowed is true, feedback app is
// opened and the os_feedback is used when the feature kOsFeedback is enabled.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       OsFeedbackIsOpenedWhenFeatureEnabled) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  base::HistogramTester histogram_tester;
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://os-feedback");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  std::string unused;
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);
  chrome::ShowFeedbackPage(browser(), chrome::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused);
  navigation_observer.Wait();

  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
}

}  // namespace ash
