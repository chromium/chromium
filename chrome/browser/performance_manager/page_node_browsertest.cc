// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/page_node.h"

#include <memory>
#include <string>

#include "base/json/string_escape.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using PageNodeBrowserTest = extensions::ExtensionBrowserTest;

}  // namespace

// Integration test verifying that the correct type is set for a PageNode
// associated with a tab.
IN_PROC_BROWSER_TEST_F(PageNodeBrowserTest, TypeTab) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_EQ(page_node->GetType(), PageType::kTab);
}

// Integration test verifying that the correct type is set for a PageNode
// associated with an extension background page.
IN_PROC_BROWSER_TEST_F(PageNodeBrowserTest, TypeExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics"));
  ASSERT_TRUE(extension);
  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->host_contents());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          host->host_contents());
  EXPECT_EQ(page_node->GetType(), PageType::kExtension);
}

namespace {

// Longer than the window during which a renderer-initiated navigation is
// treated as a client redirect of the current document.
constexpr base::TimeDelta kLongerThanClientRedirectWindow = base::Seconds(11);

// Handles /js-redirect?URL by returning a page that redirects to URL with
// location.replace().
std::unique_ptr<net::test_server::HttpResponse> HandleJsRedirect(
    const net::test_server::HttpRequest& request) {
  const GURL request_url = request.GetURL();
  if (request_url.path() != "/js-redirect") {
    return nullptr;
  }
  const std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query());
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(
      base::StrCat({"<!doctype html><script>location.replace(",
                    base::GetQuotedJSONString(dest), ");</script>"}));
  return response;
}

class PageNodeUserOrBrowserInitiatedLoadBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Tab helpers may have recorded commit times with the real clock before
    // `tick_clock_` is installed. Start from the real current time so that
    // those times aren't in the test clock's future.
    tick_clock_.SetNowTicks(base::TimeTicks::Now());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleJsRedirect));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  static bool IsUserOrBrowserInitiatedLoad(content::WebContents* contents) {
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
    CHECK(page_node);
    return page_node->IsUserOrBrowserInitiatedLoad();
  }

  // Opens `url` in a background tab from the context menu of the active tab,
  // and waits until `destination_url` commits in it.
  content::WebContents* OpenInBackgroundTabFromContextMenu(
      const GURL& url,
      const GURL& destination_url) {
    content::WebContents* opener = tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver destination_observer(destination_url);
    destination_observer.StartWatchingNewWebContents();
    std::unique_ptr<TestRenderViewContextMenu> menu =
        TestRenderViewContextMenu::Create(opener, opener->GetLastCommittedURL(),
                                          url);
    menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
    destination_observer.Wait();

    // The new tab was opened in the background.
    EXPECT_EQ(2, tab_strip_model()->count());
    EXPECT_EQ(opener, tab_strip_model()->GetActiveWebContents());
    content::WebContents* new_contents = tab_strip_model()->GetWebContentsAt(1);
    EXPECT_EQ(destination_url, new_contents->GetLastCommittedURL());
    return new_contents;
  }

  TabStripModel* tab_strip_model() { return browser()->GetTabStripModel(); }

  // Outlives the WebContents that use it.
  base::SimpleTestTickClock tick_clock_;
};

}  // namespace

// Verifies that a link opened in a background tab with a middle click (same
// path as Ctrl/Cmd+click) is considered a user-initiated load.
IN_PROC_BROWSER_TEST_F(PageNodeUserOrBrowserInitiatedLoadBrowserTest,
                       MiddleClickBackgroundTabIsUserInitiated) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* opener = tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      opener,
      "document.body.innerHTML = '<a id=\"link\" href=\"/title2.html\" "
      "style=\"display:block;width:100vw;height:100vh\">link</a>';"));
  content::WaitForHitTestData(opener->GetPrimaryMainFrame());

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  content::SimulateMouseClickAt(
      opener, /*modifiers=*/0, blink::WebMouseEvent::Button::kMiddle,
      gfx::ToFlooredPoint(
          content::GetCenterCoordinatesOfElementWithId(opener, "link")));
  content::WebContents* new_contents = tab_added_waiter.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // The new tab was opened in the background.
  ASSERT_EQ(2, tab_strip_model()->count());
  EXPECT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(embedded_test_server()->GetURL("/title2.html"),
            new_contents->GetLastCommittedURL());

  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(new_contents));
}

// Verifies that a link opened in a background tab from the context menu is
// considered a user-initiated load.
IN_PROC_BROWSER_TEST_F(PageNodeUserOrBrowserInitiatedLoadBrowserTest,
                       ContextMenuOpenLinkInNewTabIsUserInitiated) {
  const GURL opener_url = embedded_test_server()->GetURL("/title1.html");
  const GURL link_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener = tab_strip_model()->GetActiveWebContents();

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  std::unique_ptr<TestRenderViewContextMenu> menu =
      TestRenderViewContextMenu::Create(opener, opener_url, link_url);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  content::WebContents* new_contents = tab_added_waiter.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // The new tab was opened in the background.
  ASSERT_EQ(2, tab_strip_model()->count());
  EXPECT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(link_url, new_contents->GetLastCommittedURL());

  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(new_contents));
}

// Verifies that a script-initiated reload without user activation in a
// background tab is not considered a user-initiated load.
IN_PROC_BROWSER_TEST_F(PageNodeUserOrBrowserInitiatedLoadBrowserTest,
                       BackgroundRendererReloadIsNotUserInitiated) {
  content::WebContents* background_contents =
      tab_strip_model()->GetActiveWebContents();
  PerformanceManagerTabHelper::FromWebContents(background_contents)
      ->SetTickClockForTesting(&tick_clock_);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(background_contents));

  // Open another tab in the foreground.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_NE(background_contents, tab_strip_model()->GetActiveWebContents());

  // Reload long after the page loaded, so that the reload isn't treated as a
  // client redirect.
  tick_clock_.Advance(kLongerThanClientRedirectWindow);
  content::TestNavigationObserver reload_observer(background_contents);
  ASSERT_TRUE(content::ExecJs(background_contents, "location.reload();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  reload_observer.Wait();

  EXPECT_FALSE(IsUserOrBrowserInitiatedLoad(background_contents));
}

// Verifies that a meta refresh redirect in a background tab opened by the user
// continues the user-initiated load.
IN_PROC_BROWSER_TEST_F(PageNodeUserOrBrowserInitiatedLoadBrowserTest,
                       MetaRefreshRedirectInheritsUserInitiated) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");
  content::WebContents* new_contents = OpenInBackgroundTabFromContextMenu(
      embedded_test_server()->GetURL("/client-redirect?" +
                                     destination_url.spec()),
      destination_url);

  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(new_contents));
}

// Verifies that a location.replace() redirect in a background tab opened by
// the user continues the user-initiated load.
IN_PROC_BROWSER_TEST_F(PageNodeUserOrBrowserInitiatedLoadBrowserTest,
                       ScriptRedirectInheritsUserInitiated) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");
  content::WebContents* new_contents = OpenInBackgroundTabFromContextMenu(
      embedded_test_server()->GetURL("/js-redirect?" + destination_url.spec()),
      destination_url);

  EXPECT_TRUE(IsUserOrBrowserInitiatedLoad(new_contents));
}

}  // namespace performance_manager
