// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace performance_manager::user_tuning {

class ActionableTabWaiter
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  void OnActionableTabListChanged(
      PerformanceDetectionManager::ResourceType resource_type,
      std::vector<resource_attribution::PageContext> tabs) override {
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class PerformanceDetectionManagerBrowserTest : public InProcessBrowserTest {
 public:
  PerformanceDetectionManagerBrowserTest() = default;
  ~PerformanceDetectionManagerBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kPerformanceIntervention);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestingURL(std::string url = "a.com") {
    return embedded_test_server()->GetURL(url, "/title1.html");
  }

  resource_attribution::PageContext GetPageContext(int tab_index) {
    std::optional<resource_attribution::PageContext> page_context =
        resource_attribution::PageContext::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(tab_index));
    CHECK(page_context.has_value());
    return page_context.value();
  }

  PerformanceDetectionManager* manager() {
    return PerformanceDetectionManager::GetInstance();
  }

  content::WebContents* GetWebContentsAt(int index) {
    return browser()->tab_strip_model()->GetWebContentsAt(index);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceDetectionManagerBrowserTest,
                       DiscardMultiplePages) {
  ASSERT_TRUE(AddTabAtIndex(1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  std::vector<resource_attribution::PageContext> page_contexts = {
      GetPageContext(1), GetPageContext(2)};

  base::RunLoop run_loop;
  manager()->DiscardTabs(
      page_contexts,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool did_discard) {
            quit_closure.Run();
            EXPECT_TRUE(did_discard);
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(GetWebContentsAt(0)->WasDiscarded());
  EXPECT_TRUE(GetWebContentsAt(1)->WasDiscarded());
  EXPECT_TRUE(GetWebContentsAt(2)->WasDiscarded());
}

IN_PROC_BROWSER_TEST_F(PerformanceDetectionManagerBrowserTest,
                       DiscardEligibleAndClosedPage) {
  ASSERT_TRUE(AddTabAtIndex(1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));

  resource_attribution::PageContext last_page_context = GetPageContext(2);

  std::vector<resource_attribution::PageContext> page_contexts = {
      GetPageContext(1), last_page_context};

  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(last_page_context.GetWebContents(), nullptr);

  base::RunLoop run_loop;
  manager()->DiscardTabs(
      page_contexts,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool did_discard) {
            quit_closure.Run();
            EXPECT_TRUE(did_discard);
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  // The detection manager should discard the contents at index 1 even though
  // the contents at index 2 was closed.
  EXPECT_TRUE(GetWebContentsAt(1)->WasDiscarded());
}

IN_PROC_BROWSER_TEST_F(PerformanceDetectionManagerBrowserTest,
                       DiscardAllClosedPages) {
  ASSERT_TRUE(AddTabAtIndex(1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));

  std::vector<resource_attribution::PageContext> page_contexts = {
      GetPageContext(1), GetPageContext(2)};

  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);

  base::RunLoop run_loop;
  manager()->DiscardTabs(
      page_contexts,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool did_discard) {
            quit_closure.Run();
            EXPECT_FALSE(did_discard);
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PerformanceDetectionManagerBrowserTest,
                       PreventDiscardActiveTab) {
  ASSERT_TRUE(AddTabAtIndex(1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));

  std::vector<resource_attribution::PageContext> page_contexts = {
      GetPageContext(1), GetPageContext(2)};

  browser()->tab_strip_model()->ActivateTabAt(2);

  base::RunLoop run_loop;
  manager()->DiscardTabs(
      page_contexts,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool did_discard) {
            quit_closure.Run();
            EXPECT_TRUE(did_discard);
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  // The detection manager should discard the contents at index 1 even though
  // the contents at index 2 is active.
  EXPECT_TRUE(GetWebContentsAt(1)->WasDiscarded());
  EXPECT_FALSE(GetWebContentsAt(2)->WasDiscarded());
}

class PerformanceInterventionDemoModeTest
    : public PerformanceDetectionManagerBrowserTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);

    feature_list_.InitWithFeatures(
        {performance_manager::features::kPerformanceInterventionUI,
         performance_manager::features::kPerformanceInterventionDemoMode},
        {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceInterventionDemoModeTest,
                       ForceCpuRefreshNotifyObservers) {
  std::unique_ptr<ActionableTabWaiter> waiter =
      std::make_unique<ActionableTabWaiter>();
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, waiter.get());

  ASSERT_TRUE(AddTabAtIndex(1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);
  // Force the detection manager to refresh tab CPU data twice because the
  // first time the data refreshes is to establish a baseline that
  // subsequent refreshes will use to determine CPU usage.
  manager()->ForceTabCpuDataRefresh();
  manager()->ForceTabCpuDataRefresh();

  // The waiter's run loop should stop after it is notified by the performance
  // detection manager with the updated actionable tab list.
  waiter->Wait();
}

}  // namespace performance_manager::user_tuning
