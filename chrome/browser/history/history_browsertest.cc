// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using content::BrowserThread;

class HistoryBrowserTest : public InProcessBrowserTest {
 protected:
  HistoryBrowserTest() : test_server_() {
    test_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_.Start());
  }

  PrefService* GetPrefs() {
    return GetProfile()->GetPrefs();
  }

  Profile* GetProfile() {
    return browser()->profile();
  }

  std::vector<GURL> GetHistoryContents() {
    ui_test_utils::HistoryEnumerator enumerator(GetProfile());
    return enumerator.urls();
  }

  GURL GetTestUrl() {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("title2.html")));
  }

  void ExpectEmptyHistory() {
    std::vector<GURL> urls(GetHistoryContents());
    EXPECT_EQ(0U, urls.size());
  }

  void LoadAndWaitForURL(const GURL& url) {
    base::string16 expected_title(base::ASCIIToUTF16("OK"));
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void LoadAndWaitForFile(const char* filename) {
    GURL url = test_server_.GetURL(std::string("/History") + filename);
    LoadAndWaitForURL(url);
  }

  bool HistoryContainsURL(const GURL& url) { return QueryURL(url).success; }

  history::URLRow LookUpURLInHistory(const GURL& url) {
    return QueryURL(url).row;
  }

  history::QueryURLResult QueryURL(const GURL& url) {
    history::QueryURLResult query_url_result;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->QueryURL(
            url, true,
            base::BindLambdaForTesting([&](history::QueryURLResult result) {
              query_url_result = std::move(result);
              run_loop.Quit();
            }),
            &tracker);
    run_loop.Run();

    return query_url_result;
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(bool* success_out,
                         history::URLRow* url_row_out,
                         base::OnceClosure closure,
                         bool success,
                         const history::URLRow& url_row,
                         const history::VisitVector& visit_vector) {
    if (success_out)
      *success_out = success;
    if (url_row_out)
      *url_row_out = url_row;
    std::move(closure).Run();
  }

  net::EmbeddedTestServer test_server_;
};

// Test that the browser history is saved (default setting).
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabled) {
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

// Test that disabling saving browser history really works.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryDisabled) {
  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS));
  EXPECT_FALSE(HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());
  ExpectEmptyHistory();
}

// Test that changing the pref takes effect immediately
// when the browser is running.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabledThenDisabled) {
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());

  {
    // No additional entries should be present in the history.
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

// Test that changing the pref takes effect immediately
// when the browser is running.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryDisabledThenEnabled) {
  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());
  ExpectEmptyHistory();

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, false);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun(GetProfile());

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

// Disabled after fixing this test class. See http://crbug.com/511442 for
// details.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DISABLED_VerifyHistoryLength1) {
  // Test the history length for the following page transitions.
  //   -open-> Page 1.
  LoadAndWaitForFile("history_length_test_page_1.html");
}

// Disabled after fixing this test class. See http://crbug.com/511442 for
// details.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DISABLED_VerifyHistoryLength2) {
  // Test the history length for the following page transitions.
  //   -open-> Page 2 -redirect-> Page 3.
  LoadAndWaitForFile("history_length_test_page_2.html");
}

// Disabled after fixing this test class. See http://crbug.com/511442 for
// details.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DISABLED_VerifyHistoryLength3) {
  // Test the history length for the following page transitions.
  // -open-> Page 1 -> open Page 2 -redirect Page 3. open Page 4
  // -navigate_backward-> Page 3 -navigate_backward->Page 1
  // -navigate_forward-> Page 3 -navigate_forward-> Page 4
  LoadAndWaitForFile("history_length_test_page_1.html");
  LoadAndWaitForFile("history_length_test_page_2.html");
  LoadAndWaitForFile("history_length_test_page_4.html");
}

// Disabled after fixing this test class. See http://crbug.com/511442 for
// details.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest,
                       DISABLED_ConsiderRedirectAfterGestureAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 11 -slow_redirect-> Page 12.
  //
  // If redirect occurs after a user gesture, e.g., mouse click, the
  // redirect is more likely to be user-initiated rather than automatic.
  // Therefore, Page 11 should be in the history in addition to Page 12.
  LoadAndWaitForFile("history_length_test_page_11.html");

  content::SimulateMouseClick(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft);
  LoadAndWaitForFile("history_length_test_page_11.html");
}

// Disabled after fixing this test class. See http://crbug.com/511442 for
// details.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest,
                       DISABLED_ConsiderSlowRedirectAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 21 -redirect-> Page 22.
  //
  // If redirect occurs more than 5 seconds later after the page is loaded,
  // the redirect is likely to be user-initiated.
  // Therefore, Page 21 should be in the history in addition to Page 22.
  LoadAndWaitForFile("history_length_test_page_21.html");
}

// TODO(crbug.com/22111): Disabled because of flakiness and because for a while
// history didn't support #q=searchTerm. Now that it does support these type
// of URLs (crbug.com/619799), this test could be re-enabled if somebody goes
// through the effort to wait for the various stages of the page loading.
// The loading strategy of the new, Polymer version of chrome://history is
// sophisticated and multi-part, so we'd need to wait on or ensure a few things
// are happening before running the test.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DISABLED_HistorySearchXSS) {
  GURL url(std::string(chrome::kChromeUIHistoryURL) +
      "#q=%3Cimg%20src%3Dx%3Ax%20onerror%3D%22document.title%3D'XSS'%22%3E");
  ui_test_utils::NavigateToURL(browser(), url);
  // Mainly, this is to ensure we send a synchronous message to the renderer
  // so that we're not susceptible (less susceptible?) to a race condition.
  // Should a race condition ever trigger, it won't result in flakiness.
  int num = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16("<img"), true,
      true, NULL, NULL);
  EXPECT_GT(num, 0);
  EXPECT_EQ(base::ASCIIToUTF16("History"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// Verify that history persists after session restart.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, PRE_HistoryPersists) {
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, HistoryPersists) {
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

// Invalid URLs should not go in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, InvalidURLNoHistory) {
  GURL non_existant = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("non_existant_file.html"));
  ui_test_utils::NavigateToURL(browser(), non_existant);
  ExpectEmptyHistory();
}

// URLs with special schemes should not go in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, InvalidSchemeNoHistory) {
  GURL about_blank("about:blank");
  ui_test_utils::NavigateToURL(browser(), about_blank);
  ExpectEmptyHistory();
  GURL view_source("view-source:about:blank");
  ui_test_utils::NavigateToURL(browser(), view_source);
  ExpectEmptyHistory();
  GURL chrome("chrome://about");
  ui_test_utils::NavigateToURL(browser(), chrome);
  ExpectEmptyHistory();
}

// New tab page should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NewTabNoHistory) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ExpectEmptyHistory();
}

// Incognito browsing should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, IncognitoNoHistory) {
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(), GetTestUrl());
  ExpectEmptyHistory();
}

// Multiple navigations to the same url should have a single history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NavigateMultiTimes) {
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

// Verify history with multiple windows and tabs.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, MultiTabsWindowsHistory) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL url3  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title3.html")));
  GURL url4  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  Browser* browser2 = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(browser2, url2);
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, url4, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(4u, urls.size());
  ASSERT_EQ(url4, urls[0]);
  ASSERT_EQ(url3, urls[1]);
  ASSERT_EQ(url2, urls[2]);
  ASSERT_EQ(url1, urls[3]);
}

// Downloaded URLs should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DownloadNoHistory) {
  GURL download_url = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("downloads"),
      base::FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::DownloadURL(browser(), download_url);
  ExpectEmptyHistory();
}

namespace {

// Grabs the RenderFrameHost for the frame navigating to the given URL.
class RenderFrameHostGrabber : public content::WebContentsObserver {
 public:
  RenderFrameHostGrabber(content::WebContents* web_contents, const GURL& url)
      : WebContentsObserver(web_contents), url_(url) {}
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == url_) {
      render_frame_host_ = navigation_handle->GetRenderFrameHost();
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

 private:
  GURL url_;
  content::RenderFrameHost* render_frame_host_ = nullptr;
  base::RunLoop run_loop_;
};

// Simulates user clicking on a link inside the frame.
// TODO(jam): merge with content/test/content_browser_test_utils_internal.h
void NavigateFrameToURL(content::RenderFrameHost* rfh, const GURL& url) {
  content::TestFrameNavigationObserver observer(rfh);
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = rfh->GetFrameTreeNodeId();
  content::WebContents::FromRenderFrameHost(rfh)
      ->GetController()
      .LoadURLWithParams(params);
  observer.Wait();
}
}  // namespace

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, Subframe) {
  // Initial subframe requests should not show up in history.
  GURL main_page = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("page_with_iframe.html"));
  GURL initial_subframe =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("History"),
                                base::FilePath().AppendASCII("target.html"));

  RenderFrameHostGrabber rfh_grabber(
      browser()->tab_strip_model()->GetActiveWebContents(), initial_subframe);
  ui_test_utils::NavigateToURL(browser(), main_page);
  rfh_grabber.Wait();
  content::RenderFrameHost* frame = rfh_grabber.render_frame_host();
  ASSERT_TRUE(!!frame);
  ASSERT_TRUE(HistoryContainsURL(main_page));
  ASSERT_FALSE(HistoryContainsURL(initial_subframe));

  // User-initiated subframe navigations should show up in history.
  GURL manual_subframe =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("History"),
                                base::FilePath().AppendASCII("landing.html"));
  NavigateFrameToURL(frame, manual_subframe);
  ASSERT_TRUE(HistoryContainsURL(manual_subframe));

  // Page-initiated location.replace subframe navigations should not show up in
  // history.
  std::string script = "location.replace('form.html')";
  content::TestFrameNavigationObserver observer(frame);
  EXPECT_TRUE(ExecuteScript(frame, script));
  observer.Wait();
  GURL auto_subframe =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("History"),
                                base::FilePath().AppendASCII("form.html"));
  ASSERT_FALSE(HistoryContainsURL(auto_subframe));
}

// HTTP meta-refresh redirects should only have an entry for the landing page.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, RedirectHistory) {
  GURL redirector = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("redirector.html"));
  GURL landing_url = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("landing.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), redirector, 2);
  ASSERT_EQ(
      landing_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(landing_url, urls[0]);
}

// Cross-site HTTP meta-refresh redirects should only have an entry for the
// landing page.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, CrossSiteRedirectHistory) {
  // Use the default embedded_test_server() for this test in order to support a
  // cross-site redirect.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL landing_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL redirector(embedded_test_server()->GetURL(
      "bar.com", "/client-redirect?" + landing_url.spec()));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            redirector, 2);
  ASSERT_EQ(
      landing_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(landing_url, urls[0]);
}

// Verify that navigation brings current page to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NavigateBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURL(browser(), url2);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);
}

// Verify that reloading a page brings it to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, ReloadBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(tab);

  urls = GetHistoryContents();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url1, urls[0]);
  ASSERT_EQ(url2, urls[1]);
}

// Verify that back/forward brings current page to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, BackForwardBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURL(browser(), url2);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url1, urls[0]);
  ASSERT_EQ(url2, urls[1]);

  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  urls = GetHistoryContents();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);
}

// Verify that pushState() correctly sets the title of the second history entry.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, PushStateSetsTitle) {
  // Use the default embedded_test_server() for this test because pushState
  // requires a real, non-file URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::string16 title = web_contents->GetTitle();

  // Do a pushState to create a new navigation entry and a new history entry.
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     "history.pushState({},'','test.html')"));
  content::WaitForLoadStop(web_contents);

  // This should result in two history entries.
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  EXPECT_NE(urls[0], urls[1]);

  // History entry [0] is the newest one.
  history::URLRow row0 = LookUpURLInHistory(urls[0]);
  EXPECT_EQ(title, row0.title());
  history::URLRow row1 = LookUpURLInHistory(urls[1]);
  EXPECT_EQ(title, row1.title());
}

// Ensure that commits unrelated to the pending entry do not cause incorrect
// updates to history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, BeforeUnloadCommitDuringPending) {
  // Use the default embedded_test_server() for this test because replaceState
  // requires a real, non-file URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  ui_test_utils::NavigateToURL(browser(), url1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::string16 title1 = web_contents->GetTitle();

  // Create a beforeunload handler that does a replaceState during navigation,
  // unrelated to the destination URL (similar to Twitter).
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     "window.onbeforeunload = function() {"
                                     "history.replaceState({},'','test.html');"
                                     "};"));
  GURL url2(embedded_test_server()->GetURL("foo.com", "/test.html"));

  // Start a cross-site navigation to trigger the beforeunload, but don't let
  // the new URL commit yet.
  GURL url3(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  content::TestNavigationManager manager(web_contents, url3);
  web_contents->GetController().LoadURL(
      url3, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The beforeunload commit should happen before request start, which should
  // result in two history entries, with the newest in index 0. urls[0] was
  // incorrectly url3 in https://crbug.com/956208.
  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(2u, urls.size());
    EXPECT_EQ(url2, urls[0]);
    EXPECT_EQ(url1, urls[1]);
  }

  // After the pending navigation commits and the new title arrives, there
  // should be another row with the new URL and title.
  manager.WaitForNavigationFinished();
  content::WaitForLoadStop(web_contents);
  base::string16 title3 = web_contents->GetTitle();
  EXPECT_NE(title1, title3);
  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(3u, urls.size());
    EXPECT_EQ(url3, urls[0]);
    history::URLRow row0 = LookUpURLInHistory(urls[0]);
    EXPECT_EQ(title3, row0.title());

    EXPECT_EQ(url2, urls[1]);
    history::URLRow row1 = LookUpURLInHistory(urls[1]);
    EXPECT_EQ(title1, row1.title());

    EXPECT_EQ(url1, urls[2]);
    history::URLRow row2 = LookUpURLInHistory(urls[2]);
    EXPECT_EQ(title1, row2.title());
  }
}

// Verify that submitting form adds target page to history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SubmitFormAddsTargetPage) {
  GURL form = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("form.html"));
  GURL target = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("History"),
      base::FilePath().AppendASCII("target.html"));
  ui_test_utils::NavigateToURL(browser(), form);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::string16 expected_title(base::ASCIIToUTF16("Target Page"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents, "document.getElementById('form').submit()"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(target, urls[0]);
  ASSERT_EQ(form, urls[1]);
}

// Verify history shortcut opens only one history tab per window.  Also, make
// sure that existing history tab is activated.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, OneHistoryTabPerWindow) {
  GURL history_url(chrome::kChromeUIHistoryURL);

  // Even after navigate completes, the currently-active tab title is
  // 'Loading...' for a brief time while the history page loads.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::string16 expected_title(base::ASCIIToUTF16("History"));
  content::TitleWatcher title_watcher(web_contents, expected_title);
  chrome::ExecuteCommand(browser(), IDC_SHOW_HISTORY);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  chrome::ExecuteCommand(browser(), IDC_SHOW_HISTORY);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);
  ASSERT_EQ(history_url, active_web_contents->GetVisibleURL());

  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(history_url, second_tab->GetVisibleURL());
}
