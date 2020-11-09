// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"

namespace performance_manager {

class BackgroundTabLoadingBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundTabLoadingBrowserTest() {
    features_.InitAndEnableFeature(
        performance_manager::features::
            kBackgroundTabLoadingFromPerformanceManager);
    url_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));
  }
  ~BackgroundTabLoadingBrowserTest() override = default;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  void SetDefaultPropertiesForTesting(
      policies::BackgroundTabLoadingPolicy* policy) {
    // Set a value explicitly for MaxSimultaneousLoad threshold to avoid a
    // dependency on the number of cores of the machine on which the test runs.
    policy->SetMaxSimultaneousLoadsForTesting(1);
    policy->SetFreeMemoryForTesting(150);
  }
#endif

 protected:
  // Adds tabs to the given browser, all navigated to |url_|.
  void AddNTabsToBrowser(Browser* browser, int number_of_tabs_to_add) {
    int starting_tab_count = browser->tab_strip_model()->count();

    for (int i = 0; i < number_of_tabs_to_add; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + number_of_tabs_to_add, tab_count);
  }

  void EnsureTabFinishedRestoring(content::WebContents* tab) {
    content::NavigationController* controller = &tab->GetController();
    // If tab content is not in a loading state and doesn't need reload.
    if (!controller->NeedsReload() && !controller->GetPendingEntry() &&
        !controller->GetWebContents()->IsLoading()) {
      return;
    }

    EXPECT_TRUE(content::WaitForLoadStop(tab));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Configure BackgroundTabLoadingPolicy for the tests.
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([]() {
          // Set a value explicitly for thresholds that depends on system
          // information, to avoid flakiness when tests run in different
          // environments.
          policies::BackgroundTabLoadingPolicy* policy =
              policies::BackgroundTabLoadingPolicy::GetInstance();
          EXPECT_TRUE(policy);
          policy->SetMaxSimultaneousLoadsForTesting(1);
          policy->SetFreeMemoryForTesting(policies::BackgroundTabLoadingPolicy::
                                              kDesiredAmountOfFreeMemoryMb);
        }));
  }

  GURL url_;
  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
IN_PROC_BROWSER_TEST_F(BackgroundTabLoadingBrowserTest, RestoreTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser_to_restore = BrowserList::GetInstance()->get(1);

  // Add tabs and close browser.
  const int kDesiredNumberOfTabs = 3;
  AddNTabsToBrowser(
      browser_to_restore,
      kDesiredNumberOfTabs - browser_to_restore->tab_strip_model()->count());
  EXPECT_EQ(kDesiredNumberOfTabs,
            browser_to_restore->tab_strip_model()->count());
  CloseBrowserSynchronously(browser_to_restore);

  // Restore recently closed window.
  chrome::OpenWindowWithRestoredTabs(browser()->profile());
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  Browser* restored_browser = BrowserList::GetInstance()->get(1);

  EXPECT_EQ(kDesiredNumberOfTabs, restored_browser->tab_strip_model()->count());
  EXPECT_EQ(kDesiredNumberOfTabs - 1,
            restored_browser->tab_strip_model()->active_index());

  // All tabs should be loaded by BackgroundTabLoadingPolicy.
  for (int i = 0; i < kDesiredNumberOfTabs; i++) {
    EnsureTabFinishedRestoring(
        restored_browser->tab_strip_model()->GetWebContentsAt(i));
  }
}

IN_PROC_BROWSER_TEST_F(BackgroundTabLoadingBrowserTest,
                       RestoredTabsAreLoadedGradually) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser_to_restore = BrowserList::GetInstance()->get(1);

  // Add tabs and close browser.
  const int kDesiredNumberOfTabs =
      policies::BackgroundTabLoadingPolicy::kMaxTabsToLoad + 1;
  AddNTabsToBrowser(
      browser_to_restore,
      kDesiredNumberOfTabs - browser_to_restore->tab_strip_model()->count());
  EXPECT_EQ(kDesiredNumberOfTabs,
            browser_to_restore->tab_strip_model()->count());
  CloseBrowserSynchronously(browser_to_restore);

  // Restore recently closed window.
  chrome::OpenWindowWithRestoredTabs(browser()->profile());
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  Browser* restored_browser = BrowserList::GetInstance()->get(1);

  EXPECT_EQ(kDesiredNumberOfTabs, restored_browser->tab_strip_model()->count());
  EXPECT_EQ(kDesiredNumberOfTabs - 1,
            restored_browser->tab_strip_model()->active_index());

  // These tabs should be loaded by BackgroundTabLoadingPolicy.
  EnsureTabFinishedRestoring(
      restored_browser->tab_strip_model()->GetWebContentsAt(
          kDesiredNumberOfTabs - 1));
  for (int i = 0; i < kDesiredNumberOfTabs - 2; i++) {
    EnsureTabFinishedRestoring(
        restored_browser->tab_strip_model()->GetWebContentsAt(i));
  }

  // This tab shouldn't want to be loaded.
  auto* contents = restored_browser->tab_strip_model()->GetWebContentsAt(
      kDesiredNumberOfTabs - 2);
  EXPECT_FALSE(contents->IsLoading());
  EXPECT_TRUE(contents->GetController().NeedsReload());
}
#endif

}  // namespace performance_manager
