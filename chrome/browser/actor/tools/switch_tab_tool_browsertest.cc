// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace actor {

namespace {

class ActorSwitchTabToolBrowserTest : public ActorToolsTest {
 public:
  ActorSwitchTabToolBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kGlicCrossOriginNavigationGating});
  }
  ~ActorSwitchTabToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_MatchesByTitle) {
  // Tab 0 (background): title is "Title Of Awesomeness".
  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target_url));

  // Tab 1 (foreground/active): has no <title> (defaults to URL
  // "example.com/title1.html").
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            target_url);
}

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_MatchesByUrl) {
  const GURL target_url = embedded_https_test_server().GetURL(
      "example.com", "/title1.html?target_page");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target_url));

  const GURL active_url = embedded_https_test_server().GetURL(
      "example.com", "/title1.html?active_page");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("target_page");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
}

// When both the currently active tab and a single background tab match `query`,
// the active tab is ignored so the tool switches to the background tab rather
// than failing with `kPageSearchAmbiguousMatch`.
IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_IgnoresActiveTabWhenBackgroundTabMatches) {
  const GURL background_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), background_url));

  // Active tab also has title "Title Of Awesomeness" (/title2.html).
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html?fg");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
}

// When only the currently active tab matches `query`, ignoring the active tab
// leaves zero background candidates and returns
// `kPageSearchAlreadyOnMatchingTab`.
IN_PROC_BROWSER_TEST_F(
    ActorSwitchTabToolBrowserTest,
    SwitchTabTool_OnlyActiveTabMatchesReturnsAlreadyOnMatchingTab) {
  const GURL background_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), background_url));

  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_AmbiguousBackgroundMatches) {
  const GURL background_url1 =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg1");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), background_url1));

  const GURL background_url2 =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg2");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), background_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 2);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kPageSearchAmbiguousMatch);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 2);
}

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest, SwitchTabTool_NoMatch) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("nonexistent_keyword");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kPageSearchNoMatch);
}

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_EmptyQuery) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorSwitchTabToolBrowserTest,
                       SwitchTabTool_BlockedDestinationUrl) {
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), blocked_url));

  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<SwitchTabToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 1);
}

}  // namespace

}  // namespace actor
