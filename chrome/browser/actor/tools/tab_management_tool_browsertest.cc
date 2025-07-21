// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;

namespace actor {

namespace {

IN_PROC_BROWSER_TEST_F(ActorToolsTest, TabManagementTool_CreateForegroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = browser()->tab_strip_model()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeCreateTabRequest(browser()->session_id(), /*foreground=*/true);
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->GetTabCount());
  EXPECT_EQ(GURL("about:blank"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ActorToolsTest, TabManagementTool_CreateBackgroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = browser()->tab_strip_model()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeCreateTabRequest(browser()->session_id(), /*foreground=*/false);
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->GetTabCount());
  EXPECT_EQ(start_tab_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Test that the history tool correctly adds the acted on tab to the task's set
// of tabs.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TabManagementTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Create a new tab, ensure it's added to the set of acted on tabs.
  {
    std::unique_ptr<ToolRequest> action =
        MakeCreateTabRequest(browser()->session_id(), /*foreground=*/false);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(actor_task().GetTabs().size(), 1ul);

    // Since the tab was added in the background, the current tab should not
    // have been added.
    EXPECT_FALSE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
  }

  // Create a second tab, ensure it too is added to the set of acted on tabs.
  {
    std::unique_ptr<ToolRequest> action =
        MakeCreateTabRequest(browser()->session_id(), /*foreground=*/true);
    TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(actor_task().GetTabs().size(), 2ul);

    // This time the tab was created in the foreground so the active tab must be
    // in the set.
    EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
  }
}

}  // namespace
}  // namespace actor
