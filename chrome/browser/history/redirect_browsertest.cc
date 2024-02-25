// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates the browser to server and client redirect pages and makes sure
// that the correct redirects are reflected in the history database. Errors
// here might indicate that WebKit changed the calls our glue layer gets in
// the case of redirects. It may also mean problems with the history system.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event_constants.h"

class RedirectTest : public InProcessBrowserTest {
 public:
  RedirectTest() {}

  std::vector<GURL> GetRedirects(const GURL& url) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);

    // Schedule a history query for redirects. The response will be sent
    // asynchronously from the callback the history system uses to notify us
    // that it's done: OnRedirectQueryComplete.
    std::vector<GURL> rv;
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    history_service->QueryRedirectsFrom(
        url,
        base::BindOnce(&RedirectTest::OnRedirectQueryComplete,
                       base::Unretained(this), &rv, loop.QuitWhenIdleClosure()),
        &tracker_);
    loop.Run();
    return rv;
  }

 protected:
  void OnRedirectQueryComplete(std::vector<GURL>* rv,
                               base::OnceClosure quit_closure,
                               history::RedirectList redirects) {
    rv->insert(rv->end(), redirects.begin(), redirects.end());
    std::move(quit_closure).Run();
  }

  // Tracker for asynchronous history queries.
  base::CancelableTaskTracker tracker_;
};

// Tests a single server redirect
IN_PROC_BROWSER_TEST_F(RedirectTest, Server) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  GURL first_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  std::vector<GURL> redirects = GetRedirects(first_url);

  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests a single client redirect.
IN_PROC_BROWSER_TEST_F(RedirectTest, Client) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  GURL first_url =
      embedded_test_server()->GetURL("/client-redirect?" + final_url.spec());

  // The client redirect appears as two page visits in the browser.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), first_url, 2);

  std::vector<GURL> redirects = GetRedirects(first_url);

  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());

  // The address bar should display the final URL.
  EXPECT_EQ(final_url, browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());

  // Navigate one more time.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), first_url, 2);

  // The address bar should still display the final URL.
  EXPECT_EQ(final_url, browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

// http://code.google.com/p/chromium/issues/detail?id=62772
IN_PROC_BROWSER_TEST_F(RedirectTest, ClientEmptyReferer) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create the file contents, which will do a redirect to the
  // test server.
  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  ASSERT_TRUE(final_url.is_valid());
  std::string file_redirect_contents = base::StringPrintf(
      "<html>"
      "<head></head>"
      "<body onload=\"document.location='%s'\"></body>"
      "</html>",
      final_url.spec().c_str());

  // Write the contents to a temporary file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file = temp_directory.GetPath().AppendASCII("foo.html");
  ASSERT_TRUE(base::WriteFile(temp_file, file_redirect_contents));

  // Navigate to the file through the browser.
  GURL first_url = net::FilePathToFileURL(temp_file);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            first_url, 1);

  std::vector<GURL> redirects = GetRedirects(first_url);
  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests to make sure a location change when a pending redirect exists isn't
// flagged as a redirect.
IN_PROC_BROWSER_TEST_F(RedirectTest, ClientCancelled) {
  GURL first_url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("cancelled_redirect_test.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(web_contents);

  // Simulate a click to force to make a user-initiated location change;
  // otherwise, a non user-initiated in-page location change will be treated
  // as client redirect and the redirect will be recoreded, which can cause
  // this test failed.
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  navigation_observer.Wait();

  std::vector<GURL> redirects = GetRedirects(first_url);

  // There should be no redirect from first_url, because the anchor location
  // change is not considered as client redirect and the meta-refresh
  // won't have fired yet.
  ASSERT_EQ(0U, redirects.size());
  EXPECT_EQ("myanchor", web_contents->GetLastCommittedURL().ref());
}

// Tests a client->server->server redirect
IN_PROC_BROWSER_TEST_F(RedirectTest, ClientServerServer) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  GURL next_to_last =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());
  GURL second_url =
      embedded_test_server()->GetURL("/server-redirect?" + next_to_last.spec());
  GURL first_url =
      embedded_test_server()->GetURL("/client-redirect?" + second_url.spec());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), first_url, 2);

  std::vector<GURL> redirects = GetRedirects(first_url);
  ASSERT_EQ(3U, redirects.size());
  EXPECT_EQ(second_url.spec(), redirects[0].spec());
  EXPECT_EQ(next_to_last.spec(), redirects[1].spec());
  EXPECT_EQ(final_url.spec(), redirects[2].spec());
}

// Tests that the "#reference" gets preserved across server redirects.
IN_PROC_BROWSER_TEST_F(RedirectTest, ServerReference) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string ref("reference");

  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  GURL initial_url = embedded_test_server()->GetURL(
      "/server-redirect?" + final_url.spec() + "#" + ref);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  EXPECT_EQ(ref, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL()
                     .ref());
}

// Test that redirect from http:// to file:// :
// A) does not crash the browser or confuse the redirect chain, see bug 1080873
// B) does not take place.
//
// Flaky on XP and Vista, http://crbug.com/69390.
IN_PROC_BROWSER_TEST_F(RedirectTest, NoHttpToFile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL file_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("http_to_file.html"));

  GURL initial_url =
      embedded_test_server()->GetURL("/client-redirect?" + file_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  // We make sure the title doesn't match the title from the file, because the
  // nav should not have taken place.
  EXPECT_NE(u"File!",
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// Ensures that non-user initiated location changes (within page) are
// flagged as client redirects. See bug 1139823.
IN_PROC_BROWSER_TEST_F(RedirectTest, ClientFragments) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL first_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("ref_redirect.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  std::vector<GURL> redirects = GetRedirects(first_url);
  EXPECT_EQ(1U, redirects.size());
  EXPECT_EQ(first_url.spec() + "#myanchor", redirects[0].spec());
}

// TODO(timsteele): This is disabled because our current testserver can't
// handle multiple requests in parallel, making it hang on the first request
// to /slow?60. It's unable to serve our second request for /title2.html
// until /slow? completes, which doesn't give the desired behavior. We could
// alternatively load the second page from disk, but we would need to start
// the browser for this testcase with --process-per-tab, and I don't think
// we can do this at test-case-level granularity at the moment.
// http://crbug.com/45056
IN_PROC_BROWSER_TEST_F(RedirectTest,
       DISABLED_ClientCancelledByNewNavigationAfterProvisionalLoad) {
  // We want to initiate a second navigation after the provisional load for
  // the client redirect destination has started, but before this load is
  // committed. To achieve this, we tell the browser to load a slow page,
  // which causes it to start a provisional load, and while it is waiting
  // for the response (which means it hasn't committed the load for the client
  // redirect destination page yet), we issue a new navigation request.
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL("/title2.html");
  GURL slow = embedded_test_server()->GetURL("/slow?60");
  GURL first_url =
      embedded_test_server()->GetURL("/client-redirect?" + slow.spec());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents, 2);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), first_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  // We don't sleep here - the first navigation won't have been committed yet
  // because we told the server to wait a minute. This means the browser has
  // started it's provisional load for the client redirect destination page but
  // hasn't completed. Our time is now!
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), final_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  observer.Wait();

  // Check to make sure the navigation did in fact take place and we are
  // at the expected page.
  EXPECT_EQ(u"Title Of Awesomeness",
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  bool final_navigation_not_redirect = true;
  std::vector<GURL> redirects = GetRedirects(first_url);
  // Check to make sure our request for /title2.html doesn't get flagged
  // as a client redirect from the first (/client-redirect?) page.
  for (auto it = redirects.begin(); it != redirects.end(); ++it) {
    if (final_url.spec() == it->spec()) {
      final_navigation_not_redirect = false;
      break;
    }
  }
  EXPECT_TRUE(final_navigation_not_redirect);
}
