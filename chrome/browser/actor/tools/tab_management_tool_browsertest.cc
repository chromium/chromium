// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;

namespace actor {

namespace {

class ActorTabManagementToolBrowserTest : public ActorToolsTest {
 public:
  ActorTabManagementToolBrowserTest() = default;
  ~ActorTabManagementToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_CreateForegroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = browser()->tab_strip_model()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeCreateTabRequest(browser()->session_id(), /*foreground=*/true);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->GetTabCount());
  EXPECT_EQ(GURL("about:blank"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_CreateBackgroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = browser()->tab_strip_model()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeCreateTabRequest(browser()->session_id(), /*foreground=*/false);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->GetTabCount());
  EXPECT_EQ(start_tab_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Test that the history tool correctly adds the acted on tab to the task's set
// of tabs.
IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Create a new tab, ensure it's added to the set of acted on tabs.
  {
    std::unique_ptr<ToolRequest> action =
        MakeCreateTabRequest(browser()->session_id(), /*foreground=*/false);
    ActResultFuture result;
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
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(actor_task().GetTabs().size(), 2ul);

    // This time the tab was created in the foreground so the active tab must be
    // in the set.
    EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
  }
}

// This test ensures that the CreateTab action waits long enough after acting
// for a screenshot to be successfully taken.
IN_PROC_BROWSER_TEST_F(
    ActorTabManagementToolBrowserTest,
    TabManagementTool_CreateForegroundTabAndEnsureScreenshotIsTaken) {
  const GURL start_tab_url =
      embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  std::unique_ptr<ToolRequest> action =
      MakeCreateTabRequest(browser()->session_id(), /*foreground=*/true);
  ActResultFuture act_result;
  actor_task().Act(ToRequestList(action), act_result.GetCallback());
  ExpectOkResult(act_result);

  ActorKeyedService* actor_keyed_service =
      ActorKeyedService::Get(browser()->profile());

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service->RequestTabObservation(
      *tabs::TabInterface::GetFromContents(web_contents()), actor_task().id(),
      future.GetCallback());

  const ActorKeyedService::TabObservationResult& observation_result =
      future.Get();
  ASSERT_TRUE(observation_result.has_value());
  ASSERT_TRUE(observation_result.value()->screenshot_result.has_value());
}

}  // namespace
}  // namespace actor
