// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace actor {

namespace {

class ActorOpenKnownPageToolBrowserTest : public ActorToolsTest {
 public:
  ActorOpenKnownPageToolBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kGlicCrossOriginNavigationGating});
  }
  ~ActorOpenKnownPageToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void AddHistoryEntry(const GURL& url, const std::u16string& title) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);
    history::HistoryAddPageArgs args;
    args.url = url;
    args.title = title;
    args.time = base::Time::Now();
    history_service->AddPage(args);
    history::BlockUntilHistoryProcessesPendingRequests(history_service);
  }

  void AddBookmarkEntry(const GURL& url, const std::u16string& title) {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    ASSERT_TRUE(bookmark_model);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmark_model->AddURL(bookmark_model->other_node(), 0, title, url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that if an open background tab and a history entry both match the
// query, the cascade prioritizes Stage 1 (Tabs) and activates the existing tab
// rather than navigating to the history entry.
IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest, TestCascadeTab) {
  // Tab 0 (background): title is "Title Of Awesomeness".
  const GURL tab_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html?tab");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), tab_url));

  // Add a competing History entry and Bookmark that also match "Awesomeness".
  const GURL history_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?history");
  AddHistoryEntry(history_url, u"Title Of Awesomeness History");
  const GURL bookmark_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?bookmark");
  AddBookmarkEntry(bookmark_url, u"Title Of Awesomeness Bookmark");

  // Tab 1 (foreground/active): has no <title> (defaults to URL
  // "example.com/title1.html").
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            tab_url);
}

// Ensures that if no open tab matches, but a history entry matches, the cascade
// resolves Stage 2 (History) and navigates the active tab to the history URL
// (even if a bookmark also matches).
IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest, TestCascadeHistory) {
  // Active tab: has no <title> (defaults to URL "example.com/title1.html").
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  const GURL history_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?from_history");
  AddHistoryEntry(history_url, u"UniqueHistoryPage");

  const GURL bookmark_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?from_bookmark");
  AddBookmarkEntry(bookmark_url, u"UniqueHistoryPage Bookmark");

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("UniqueHistoryPage");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            history_url);
}

// Ensures that if neither open tabs nor history match, the cascade falls back
// to Stage 3 (Bookmarks) and navigates the active tab to the bookmark URL.
IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest, TestCascadeBookmark) {
  // Active tab: has no <title> (defaults to URL "example.com/title1.html").
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  const GURL bookmark_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?from_bookmark");
  AddBookmarkEntry(bookmark_url, u"SavedBookmarkTarget");

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("SavedBookmarkTarget");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            bookmark_url);
}

// Ensures that when all three stages (Tabs, History, Bookmarks) return empty,
// the tool fails with `kPageSearchNoMatch`.
IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest, TestNoMatch) {
  // Active tab: has no <title> (defaults to URL "example.com/title1.html").
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("nonexistent_keyword");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kPageSearchNoMatch);
}

// When only the currently active tab matches `query`, the tool returns
// `kPageSearchAlreadyOnMatchingTab` rather than falling through to History or
// Bookmarks and redundantly re-navigating the active tab.
IN_PROC_BROWSER_TEST_F(
    ActorOpenKnownPageToolBrowserTest,
    OpenKnownPageTool_OnlyActiveTabMatchesReturnsAlreadyOnMatchingTab) {
  // Active tab: title is "Title Of Awesomeness" (/title2.html).
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), active_url));

  // Also add matching history and bookmark entries. This explicitly tests that
  // when the active tab matches in Stage 1, the cascade halts immediately and
  // does not fall through to re-navigate the active tab via Stage 2 or Stage 3.
  const GURL history_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?history");
  AddHistoryEntry(history_url, u"Title Of Awesomeness History");
  const GURL bookmark_url = embedded_https_test_server().GetURL(
      "example.com", "/title2.html?bookmark");
  AddBookmarkEntry(bookmark_url, u"Title Of Awesomeness Bookmark");

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab);
}

IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest,
                       OpenKnownPageTool_AmbiguousBackgroundTabMatches) {
  // Tab 0 (background): title is "Title Of Awesomeness".
  const GURL background_url1 =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg1");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), background_url1));

  // Tab 1 (background): title is also "Title Of Awesomeness".
  const GURL background_url2 =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg2");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), background_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Tab 2 (foreground/active): has no <title> (defaults to URL
  // "example.com/title1.html").
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 2);

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("Awesomeness");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kPageSearchAmbiguousMatch);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 2);
}

IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest,
                       OpenKnownPageTool_EmptyQuery) {
  // Active tab: has no <title> (defaults to URL "example.com/title1.html").
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest,
                       OpenKnownPageTool_WhitespaceQuery) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("   ");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(
    ActorOpenKnownPageToolBrowserTest,
    OpenKnownPageTool_IgnoresActiveTabWhenBackgroundTabMatches) {
  // Tab 0 (background): title is "Title Of Awesomeness".
  const GURL background_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html?bg");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), background_url));

  // Tab 1 (foreground/active): also has title "Title Of Awesomeness".
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html?fg");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  // Use leading and trailing whitespace to also verify that trimmed queries
  // match properly.
  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("   Awesomeness   ");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            background_url);
}

IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest,
                       OpenKnownPageTool_BlockedDestinationUrl) {
  // Active tab: has no <title> (defaults to URL "example.com/title1.html").
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/title2.html");
  AddBookmarkEntry(blocked_url, u"BlockedBookmarkPage");

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("BlockedBookmarkPage");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);

  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            start_url);
}

IN_PROC_BROWSER_TEST_F(ActorOpenKnownPageToolBrowserTest,
                       OpenKnownPageTool_NewTabPageAllowed) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  const GURL ntp_url("chrome://newtab/");
  AddBookmarkEntry(ntp_url, u"New Tab Page Bookmark");

  std::unique_ptr<ToolRequest> action =
      std::make_unique<OpenKnownPageToolRequest>("New Tab Page Bookmark");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            ntp_url);
}

}  // namespace

}  // namespace actor
