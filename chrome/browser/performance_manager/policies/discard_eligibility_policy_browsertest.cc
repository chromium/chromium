// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::policies {

using DiscardReason = DiscardEligibilityPolicy::DiscardReason;
using CanDiscardResult::kEligible;
using CanDiscardResult::kProtected;
using performance_manager::testing::ExpectCanDiscardEligibleAllReasons;
using performance_manager::testing::ExpectCanDiscardProtected;

namespace {

class DiscardEligibilityPolicyBrowserTest : public InProcessBrowserTest {
 protected:
  DiscardEligibilityPolicyBrowserTest() = default;
  ~DiscardEligibilityPolicyBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestingURL() { return embedded_test_server()->GetURL("/empty.html"); }
};

// Disable CannotDiscardVisibleInSplit on Chrome OS build because it's flaky
// on linux-chromeos-chrome, bots that aren't on the CQ.
// TODO(crbug.com/464057202): Try to enable this test on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CannotDiscardVisibleInSplit DISABLED_CannotDiscardVisibleInSplit
#else
#define MAYBE_CannotDiscardVisibleInSplit CannotDiscardVisibleInSplit
#endif

IN_PROC_BROWSER_TEST_F(DiscardEligibilityPolicyBrowserTest,
                       MAYBE_CannotDiscardVisibleInSplit) {
  // Open tabs for testing.
  const int index1 = 1, index2 = 2, index3 = 3;
  ASSERT_TRUE(
      AddTabAtIndex(index1, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(index2, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(index3, GetTestingURL(), ui::PAGE_TRANSITION_TYPED));

  // Setup the split view.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(index1);
  tab_strip_model->AddToNewSplit(
      {index2}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  PageNode* page_node1 = PerformanceManager::GetPrimaryPageNodeForWebContents(
                             tab_strip_model->GetWebContentsAt(index1))
                             .get();
  ASSERT_TRUE(page_node1);
  PageNode* page_node2 = PerformanceManager::GetPrimaryPageNodeForWebContents(
                             tab_strip_model->GetWebContentsAt(index2))
                             .get();
  ASSERT_TRUE(page_node2);

  // When another tab is activated, both tabs in the split view can be
  // discarded.
  tab_strip_model->ActivateTabAt(index3);
  ExpectCanDiscardEligibleAllReasons(page_node1, base::TimeDelta());
  ExpectCanDiscardEligibleAllReasons(page_node2, base::TimeDelta());

  // When a tab in the split view is activated, both tabs in the split view can
  // not be discarded.
  tab_strip_model->ActivateTabAt(index1);
  ExpectCanDiscardProtected(page_node1,
                            {DiscardReason::URGENT, DiscardReason::PROACTIVE,
                             DiscardReason::SUGGESTED},
                            CannotDiscardReason::kVisible);
  ExpectCanDiscardProtected(page_node2, {DiscardReason::URGENT},
                            CannotDiscardReason::kVisible);
  tab_strip_model->ActivateTabAt(index2);
  ExpectCanDiscardProtected(page_node1,
                            {DiscardReason::URGENT, DiscardReason::PROACTIVE,
                             DiscardReason::SUGGESTED},
                            CannotDiscardReason::kVisible);
  ExpectCanDiscardProtected(page_node2, {DiscardReason::URGENT},
                            CannotDiscardReason::kVisible);

  // When another tab is activated, both tabs in the split view can be
  // discarded.
  tab_strip_model->ActivateTabAt(index3);
  ExpectCanDiscardEligibleAllReasons(page_node1, base::TimeDelta());
  ExpectCanDiscardEligibleAllReasons(page_node2, base::TimeDelta());
}

// Test DiscardEligibilityPolicy behavior with web application.
class DiscardEligibilityPolicyWebAppBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  constexpr static std::string_view kTestAppUrl =
      "https://www.example.com/app/";

  DiscardEligibilityPolicyWebAppBrowserTest() = default;

  // Convenience wrappers for DiscardEligibilityPolicy::CanDiscard().
  CanDiscardResult CanDiscard(
      const PageNode* page_node,
      DiscardReason discard_reason,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) {
    return DiscardEligibilityPolicy::GetFromGraph(page_node->GetGraph())
        ->CanDiscard(page_node, discard_reason,
                     kNonVisiblePagesUrgentProtectionTime,
                     cannot_discard_reasons);
  }
};

IN_PROC_BROWSER_TEST_F(DiscardEligibilityPolicyWebAppBrowserTest,
                       CannotDiscardWebApp) {
  // Set up the web application.
  webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "App", GURL(kTestAppUrl));
  ASSERT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
            base::ok());

  content::WebContents* browser_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for and get the web contents that was loaded.
  ui_test_utils::UrlLoadObserver url_observer((GURL(kTestAppUrl)));
  std::string script = content::JsReplace(
      R"(window.open($1, '_blank', 'noopener');)", kTestAppUrl);
  EXPECT_TRUE(content::ExecJs(browser_tab, script));
  url_observer.Wait();
  content::WebContents* contents = url_observer.web_contents();

  EXPECT_TRUE(
      web_app::WebAppTabHelper::FromWebContents(contents)->is_in_app_window());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  ASSERT_TRUE(page_node);

  // Check CanDiscard results.
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node.get(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(base::Contains(reasons_vec, CannotDiscardReason::kWebApp));

  reasons_vec.clear();
  EXPECT_EQ(kProtected, CanDiscard(page_node.get(), DiscardReason::PROACTIVE,
                                   &reasons_vec));
  EXPECT_TRUE(base::Contains(reasons_vec, CannotDiscardReason::kWebApp));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node.get(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

}  // namespace

}  // namespace performance_manager::policies
