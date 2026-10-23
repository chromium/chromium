// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/actor.mojom.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/base_window.h"

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

  BrowserWindowInterface* browser_window() {
    return active_tab()->GetBrowserWindowInterface();
  }

#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* CreateIncognitoBrowserWindow() {
    Profile* incognito_profile =
        GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    TestFuture<BrowserWindowInterface*> future;
    CreateBrowserWindow(BrowserWindowCreateParams(*incognito_profile,
                                                  /*from_user_gesture=*/true),
                        future.GetCallback());
    BrowserWindowInterface* incognito_browser = future.Get();
    CHECK(incognito_browser);
    TabListInterface* incognito_tab_list =
        TabListInterface::From(incognito_browser);
    if (incognito_tab_list->GetTabCount() == 0) {
      incognito_tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
    }
    return incognito_browser;
  }
#endif
};

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_CreateForegroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = tab_list()->GetTabCount();

  std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
      browser_window()->GetSessionID(), /*foreground=*/true);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, tab_list()->GetTabCount());
  EXPECT_EQ(GURL("about:blank"),
            tab_list()->GetActiveTab()->GetContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_CreateBackgroundTab) {
  // Navigate the starting tab so it can be differentiated from the new tab.
  const GURL start_tab_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  const int initial_tab_count = tab_list()->GetTabCount();

  std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
      browser_window()->GetSessionID(), /*foreground=*/false);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(initial_tab_count + 1, tab_list()->GetTabCount());
  EXPECT_EQ(start_tab_url, tab_list()->GetActiveTab()->GetContents()->GetURL());
}

#if !BUILDFLAG(IS_ANDROID)
// Ensure CreateTab fails when targeting a window that belongs to a different
// profile and that no tab is added to the task.
IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       CreateTabRejectsOtherProfileWindow) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowserWindow();
  TabListInterface* incognito_tab_list =
      TabListInterface::From(incognito_browser);
  const int incognito_tab_count = incognito_tab_list->GetTabCount();

  std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
      incognito_browser->GetSessionID(), /*foreground=*/true);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kActionTargetCrossProfile);

  EXPECT_EQ(incognito_tab_count, incognito_tab_list->GetTabCount());
  EXPECT_TRUE(actor_task().GetTabs().empty());

  incognito_browser->GetWindow()->Close();
}

// Ensure a tab in another profile can't be closed. Requests target actor
// surfaces, and the tab has none, so it can't be targeted at all; a request for
// an actor surface that doesn't exist fails without closing anything.
IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       CloseTabRejectsOtherProfileTab) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowserWindow();
  TabListInterface* incognito_tab_list =
      TabListInterface::From(incognito_browser);
  tabs::TabInterface* incognito_tab = incognito_tab_list->GetActiveTab();
  const int incognito_tab_count = incognito_tab_list->GetTabCount();

  // The incognito profile has no ActorKeyedService, so its tab has no actor
  // surface to target.
  EXPECT_TRUE(ActorSurfaceHandle::From(incognito_tab->GetHandle()).is_null());

  std::unique_ptr<ToolRequest> action = std::make_unique<CloseTabToolRequest>(
      ActorSurfaceHandle::CreateForTesting());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);

  EXPECT_EQ(incognito_tab_count, incognito_tab_list->GetTabCount());

  incognito_browser->GetWindow()->Close();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Test that the history tool correctly adds the acted on tab to the task's set
// of tabs.
IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       TabManagementTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Create a new tab, ensure it's added to the set of acted on tabs.
  {
    std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
        browser_window()->GetSessionID(), /*foreground=*/false);
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
    std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
        browser_window()->GetSessionID(), /*foreground=*/true);
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
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
      browser_window()->GetSessionID(), /*foreground=*/true);
  ActResultFuture act_result;
  actor_task().Act(ToRequestList(action), act_result.GetCallback());
  ExpectOkResult(act_result);

  ActorKeyedService* actor_keyed_service = ActorKeyedService::Get(GetProfile());

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service->RequestTabObservation(
      *active_tab(), actor_task().id(), std::nullopt, future.GetCallback());

  const ActorKeyedService::TabObservationResult& observation_result =
      future.Get();
  ASSERT_TRUE(observation_result.has_value());
  ASSERT_TRUE(observation_result.value()->screenshot_result.has_value());
}

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest, ActivateTab) {
  // Navigate the first tab.
  const GURL start_tab_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  // Create a second tab in the foreground.
  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(start_tab_url));
  ASSERT_EQ(tab_list()->GetTabCount(), 2);
  ASSERT_EQ(tab_list()->GetTab(1), tab_list()->GetActiveTab());

  // Use a TabManagementTool to activate the first tab again, which should
  // bring it to the foreground.
  std::unique_ptr<ToolRequest> action =
      MakeActivateTabRequest(tab_list()->GetTab(0)->GetHandle());
  ActResultFuture act_result;
  actor_task().Act(ToRequestList(action), act_result.GetCallback());
  ExpectOkResult(act_result);

  ASSERT_EQ(tab_list()->GetTabCount(), 2);
  ASSERT_EQ(tab_list()->GetTab(0), tab_list()->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest, CloseTab) {
  // Navigate the first tab.
  const GURL start_tab_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_tab_url));

  // Create a second tab in the foreground.
  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(start_tab_url));
  ASSERT_EQ(tab_list()->GetTabCount(), 2);
  ASSERT_EQ(tab_list()->GetTab(1), tab_list()->GetActiveTab());

  // Use a TabManagementTool to close the inactive tab.
  std::unique_ptr<ToolRequest> action =
      MakeCloseTabRequest(tab_list()->GetTab(0)->GetHandle());
  ActResultFuture act_result;
  actor_task().Act(ToRequestList(action), act_result.GetCallback());
  ExpectOkResult(act_result);

  ASSERT_EQ(tab_list()->GetTabCount(), 1);
  ASSERT_EQ(tab_list()->GetTab(0), tab_list()->GetActiveTab());
}

// Ensures that if a tab is closed, it's properly removed from the list of tabs
// managed by the ActorTask.
IN_PROC_BROWSER_TEST_F(ActorTabManagementToolBrowserTest,
                       CloseTabRemovesFromActorTask) {
  // Create a new tab, ensure it's added to the set of acted on tabs.
  {
    std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
        browser_window()->GetSessionID(), /*foreground=*/false);
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
    std::unique_ptr<ToolRequest> action = MakeCreateTabRequest(
        browser_window()->GetSessionID(), /*foreground=*/true);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(actor_task().GetTabs().size(), 2ul);

    // This time the tab was created in the foreground so the active tab must be
    // in the set.
    EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
  }

  {
    tabs::TabHandle closed_tab_handle = active_tab()->GetHandle();
    // Use a TabManagementTool to close the active tab.
    std::unique_ptr<ToolRequest> action =
        MakeCloseTabRequest(closed_tab_handle);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    // The closed tab should no longer be managed by the actor.
    EXPECT_EQ(actor_task().GetTabs().size(), 1ul);

    // Because the active tab was closed, the new active tab should be
    // different.
    EXPECT_NE(closed_tab_handle, active_tab()->GetHandle());
  }
}

}  // namespace
}  // namespace actor
