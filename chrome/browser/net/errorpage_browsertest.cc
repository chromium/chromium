// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "components/policy/core/common/policy_types.h"
#else
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#endif
#include "components/policy/core/common/mock_configuration_policy_provider.h"

using content::BrowserThread;
using content::NavigationController;
using net::URLRequestFailedJob;
using net::URLRequestTestJob;

namespace {

// Searches for first node containing |text|, and if it finds one, searches
// through all ancestors seeing if any of them is of class "hidden". Since it
// relies on the hidden class used by network error pages, not suitable for
// general use.
bool WARN_UNUSED_RESULT IsDisplayingText(Browser* browser,
                                         const std::string& text) {
  // clang-format off
  std::string command = base::StringPrintf(R"(
    function isNodeVisible(node) {
      if (!node || node.classList.contains('hidden'))
        return false;
      if (!node.parentElement)
        return true;
      // Otherwise, we must check all parent nodes
      return isNodeVisible(node.parentElement);
    }
    var node = document.evaluate("//*[contains(text(),'%s')]", document,
      null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
    domAutomationController.send(isNodeVisible(node));
  )", text.c_str());
  // clang-format on
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(), command, &result));
  return result;
}

// Expands the more box on the currently displayed error page.
void ToggleHelpBox(Browser* browser) {
  EXPECT_TRUE(content::ExecuteScript(
      browser->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('details-button').click();"));
}

// Returns true if the diagnostics link suggestion is displayed.
bool WARN_UNUSED_RESULT IsDisplayingDiagnosticsLink(Browser* browser) {
  std::string command = base::StringPrintf(
      "var diagnose_link = document.getElementById('diagnose-link');"
      "domAutomationController.send(diagnose_link != null);");
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(), command, &result));
  return result;
}

// Checks that the local error page is being displayed, without remotely
// retrieved navigation corrections, and with the specified error string.
void ExpectDisplayingLocalErrorPage(const std::string& url,
                                    Browser* browser,
                                    const std::string& error_string) {
  EXPECT_TRUE(IsDisplayingText(browser, error_string));

  // Locally generated error pages should not have navigation corrections.
  EXPECT_FALSE(IsDisplayingText(browser, url));

  // Locally generated error pages should not have a link with search terms.
  EXPECT_FALSE(IsDisplayingText(browser, "search query"));
}

// Checks that the local error page is being displayed, without remotely
// retrieved navigation corrections, and with the specified error code.
void ExpectDisplayingLocalErrorPage(const std::string& url,
                                    Browser* browser,
                                    net::Error error_code) {
  ExpectDisplayingLocalErrorPage(url, browser,
                                 net::ErrorToShortString(error_code));
}

// Checks that an error page with information retrieved from the navigation
// correction service is being displayed, with the specified specified error
// string.
void ExpectDisplayingNavigationCorrections(const std::string& url,
                                           Browser* browser,
                                           const std::string& error_string) {
  EXPECT_TRUE(IsDisplayingText(browser, error_string));

  // Check that the mock navigation corrections are displayed.
  EXPECT_TRUE(IsDisplayingText(browser, url));

  // Check that the search terms are displayed as a link.
  EXPECT_TRUE(IsDisplayingText(browser, "search query"));

  // The diagnostics button isn't displayed when corrections were
  // retrieved from a remote server.
  EXPECT_FALSE(IsDisplayingDiagnosticsLink(browser));
}

// Checks that an error page with information retrieved from the navigation
// correction service is being displayed, with the specified specified error
// code.
void ExpectDisplayingNavigationCorrections(const std::string& url,
                                           Browser* browser,
                                           net::Error error_code) {
  ExpectDisplayingNavigationCorrections(url, browser,
                                        net::ErrorToShortString(error_code));
}

std::string GetShowSavedButtonLabel() {
  return l10n_util::GetStringUTF8(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY);
}

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  ErrorPageTest() = default;
  ~ErrorPageTest() override = default;

  // Navigates the active tab to a mock url created for the file at |file_path|.
  // Needed for StaleCacheStatus and StaleCacheStatusFailedCorrections tests.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        error_page::switches::kShowSavedCopy,
        error_page::switches::kEnableShowSavedCopyPrimary);
  }

  // Navigates the active tab to a mock url created for the file at |path|.
  void NavigateToFileURL(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int32_t num_navigations) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  // Navigates back in the history and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void GoBackAndWaitForTitle(const std::string& expected_title,
                             int32_t num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_BACK);
  }

  // Navigates forward in the history and waits for |num_navigations| to occur,
  // and the title to change to |expected_title|.
  void GoForwardAndWaitForTitle(const std::string& expected_title,
                                int32_t num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_FORWARD);
  }

  void GoBackAndWaitForNavigations(int32_t num_navigations) {
    NavigateHistory(num_navigations, HISTORY_NAVIGATE_BACK);
  }

  void GoForwardAndWaitForNavigations(int32_t num_navigations) {
    NavigateHistory(num_navigations, HISTORY_NAVIGATE_FORWARD);
  }

  // Navigates the browser the indicated direction in the history and waits for
  // |num_navigations| to occur and the title to change to |expected_title|.
  void NavigateHistoryAndWaitForTitle(const std::string& expected_title,
                                      int32_t num_navigations,
                                      HistoryNavigationDirection direction) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    NavigateHistory(num_navigations, direction);

    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              base::ASCIIToUTF16(expected_title));
  }

  void NavigateHistory(int32_t num_navigations,
                       HistoryNavigationDirection direction) {
    content::TestNavigationObserver test_navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), num_navigations);
    if (direction == HISTORY_NAVIGATE_BACK) {
      chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
    } else {
      FAIL();
    }
    test_navigation_observer.Wait();
  }

  // Confirms that the javascript variable indicating whether or not we have
  // a stale copy in the cache has been set to |expected|, and that the
  // stale load button is or isn't there based on the same expectation.
  testing::AssertionResult ProbeStaleCopyValue(bool expected) {
    const char* js_cache_probe =
        "try {\n"
        "    domAutomationController.send(\n"
        "        loadTimeData.valueExists('showSavedCopyButton') ?"
        "            'yes' : 'no');\n"
        "} catch (e) {\n"
        "    domAutomationController.send(e.message);\n"
        "}\n";

    std::string result;
    bool ret =
        content::ExecuteScriptAndExtractString(
            browser()->tab_strip_model()->GetActiveWebContents(),
            js_cache_probe,
            &result);
    if (!ret) {
      return testing::AssertionFailure()
          << "Failing return from ExecuteScriptAndExtractString.";
    }

    if ((expected && "yes" == result) || (!expected && "no" == result))
      return testing::AssertionSuccess();

    return testing::AssertionFailure() << "Cache probe result is " << result;
  }

  testing::AssertionResult ReloadStaleCopyFromCache() {
    const char* js_reload_script =
        "try {\n"
        "    document.getElementById('show-saved-copy-button').click();\n"
        "    domAutomationController.send('success');\n"
        "} catch (e) {\n"
        "    domAutomationController.send(e.message);\n"
        "}\n";

    std::string result;
    bool ret = content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        js_reload_script,
        &result);
    EXPECT_TRUE(ret);
    if (!ret)
      return testing::AssertionFailure();
    return ("success" == result ? testing::AssertionSuccess() :
            (testing::AssertionFailure() << "Exception message is " << result));
  }
};

class TestFailProvisionalLoadObserver : public content::WebContentsObserver {
 public:
  explicit TestFailProvisionalLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  ~TestFailProvisionalLoadObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsErrorPage())
      fail_url_ = navigation_handle->GetURL();
  }

  const GURL& fail_url() const { return fail_url_; }

 private:
  GURL fail_url_;

  DISALLOW_COPY_AND_ASSIGN(TestFailProvisionalLoadObserver);
};

class DNSErrorPageTest : public ErrorPageTest {
 public:
  DNSErrorPageTest() {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](DNSErrorPageTest* owner,
               content::URLLoaderInterceptor::RequestParams* params) {
              // Add an interceptor that serves LinkDoctor responses
              if (google_util::LinkDoctorBaseURL() == params->url_request.url) {
                // Send RequestCreated so that anyone blocking on
                // WaitForRequests can continue.
                base::PostTaskWithTraits(
                    FROM_HERE, {BrowserThread::UI},
                    base::BindOnce(&DNSErrorPageTest::RequestCreated,
                                   base::Unretained(owner)));
                return chrome_browser_net::WriteFileToURLLoader(
                    owner->embedded_test_server(), params,
                    "mock-link-doctor.json");
              }

              // Add an interceptor for the search engine the error page will
              // use.
              if (params->url_request.url.host() ==
                  owner->search_term_url_.host()) {
                return chrome_browser_net::WriteFileToURLLoader(
                    owner->embedded_test_server(), params, "title3.html");
              }

              return false;
            },
            this));
  }

  ~DNSErrorPageTest() override = default;

  // When it sees a request for |path|, returns a 500 response with a body that
  // will be sniffed as binary/octet-stream.
  static std::unique_ptr<net::test_server::HttpResponse>
  Return500WithBinaryBody(const std::string& path,
                          const net::test_server::HttpRequest& request) {
    if (path != request.relative_url)
      return nullptr;
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("HTTP/1.1 500 Server Sad :(\n\n",
                                              "\x01"));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void SetUpOnMainThread() override {
    // All mock.http requests get served by the embedded test server.
    host_resolver()->AddRule("mock.http", "127.0.0.1");

    ASSERT_TRUE(embedded_test_server()->Start());

    UIThreadSearchTermsData search_terms_data(browser()->profile());
    search_term_url_ = GURL(search_terms_data.GoogleBaseURLValue());
  }

  void WaitForRequests(int32_t requests_to_wait_for) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(-1, requests_to_wait_for_);
    DCHECK(!run_loop_);

    if (requests_to_wait_for <= num_requests_)
      return;

    requests_to_wait_for_ = requests_to_wait_for;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
    requests_to_wait_for_ = -1;
    EXPECT_EQ(num_requests_, requests_to_wait_for);
  }

  // Returns the total number of requests handled thus far.
  int32_t num_requests() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return num_requests_;
  }

  void RequestCreated() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    num_requests_++;
    if (num_requests_ == requests_to_wait_for_)
      run_loop_->Quit();
  }

  // Returns a GURL that results in a DNS error.
  GURL GetDnsErrorURL() const {
    return URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  }

 private:
  // These are only used on the UI thread.
  int32_t num_requests_ = 0;
  int32_t requests_to_wait_for_ = -1;
  GURL search_term_url_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Test an error with a file URL, and make sure it doesn't have a
// button to launch a network diagnostics tool.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, FileNotFound) {
  // Create an empty temp directory, to be sure there's no file in it.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  GURL non_existent_file_url =
      net::FilePathToFileURL(temp_dir.GetPath().AppendASCII("marmoset"));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), non_existent_file_url, 1);

  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_FILE_NOT_FOUND);
  // Should not request Link Doctor corrections for local errors.
  EXPECT_EQ(0, num_requests());
  // Only errors on HTTP/HTTPS pages should display a diagnostics button.
  EXPECT_FALSE(IsDisplayingDiagnosticsLink(browser()));
}

// Check an network error page for ERR_FAILED. In particular, this should
// not trigger a link doctor error page.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Failed) {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), URLRequestFailedJob::GetMockHttpUrl(net::ERR_FAILED), 1);

  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_FAILED);
  // Should not request Link Doctor corrections for this error.
  EXPECT_EQ(0, num_requests());
}

// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_Basic) {
  // The first navigation should fail and load a blank page, while it fetches
  // the Link Doctor response.  The second navigation is the Link Doctor.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack1) {
  NavigateToFileURL("/title2.html");
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(1, num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL("/title2.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  NavigateToFileURL("/title3.html");

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, num_requests());

  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(2, num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL("/title2.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);

  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  NavigateToFileURL("/title3.html");

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, num_requests());

  GoBackAndWaitForTitle("Title Of Awesomeness", 1);

  GoForwardAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, num_requests());
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2Forward2) {
  NavigateToFileURL("/title3.html");

  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  NavigateToFileURL("/title2.html");

  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(2, num_requests());

  GoBackAndWaitForTitle("Title Of More Awesomeness", 1);

  GoForwardAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, num_requests());

  GoForwardAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(3, num_requests());
}

// Test that the search link on a DNS error page works.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_DoSearch) {
  // The first navigation should fail, and the second one should be the error
  // page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Do a search and make sure the browser ends up at the right page.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  content::TitleWatcher title_watcher(
      web_contents,
      base::ASCIIToUTF16("Title Of More Awesomeness"));
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('search-link').click();"));
  nav_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Title Of More Awesomeness"),
            title_watcher.WaitAndGetTitle());

  // There should have been another Link Doctor request, for tracking purposes.
  // Have to wait for it, since the search page does not depend on having
  // sent the tracking request.
  WaitForRequests(2);

  // Check the path and query string.
  std::string url;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
                  browser()->tab_strip_model()->GetActiveWebContents(),
                  "domAutomationController.send(window.location.href);",
                  &url));
  EXPECT_EQ("/search", GURL(url).path());
  EXPECT_EQ("q=search%20query", GURL(url).query());

  // Go back to the error page, to make sure the history is correct.
  GoBackAndWaitForNavigations(2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(3, num_requests());
}

// Test that the reload button on a DNS error page works.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_DoReload) {
  // The first navigation should fail, and the second one should be the error
  // page.
  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Clicking the reload button should load the error page again, and there
  // should be two commits, as before.
  content::TestNavigationObserver nav_observer(web_contents, 2);
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer.Wait();
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);

  // There should have been two more requests to the correction service:  One
  // for the new error page, and one for tracking purposes.  Have to make sure
  // to wait for the tracking request, since the new error page does not depend
  // on it.
  WaitForRequests(3);
}

// Test that the reload button on a DNS error page works after a same document
// navigation on the error page.  Error pages don't seem to do this, but some
// traces indicate this may actually happen.  This test may hang on regression.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest,
                       DNSError_DoReloadAfterSameDocumentNavigation) {
  // The first navigation should fail, and the second one should be the error
  // page.
  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Do a same-document navigation on the error page, which should not result
  // in a new navigation.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.location='#';"));
  content::WaitForLoadStop(web_contents);
  // Page being displayed should not change.
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);
  // No new requests should have been issued.
  EXPECT_EQ(1, num_requests());

  // Clicking the reload button should load the error page again, and there
  // should be two commits, as before.
  content::TestNavigationObserver nav_observer2(web_contents, 2);
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer2.Wait();
  ExpectDisplayingNavigationCorrections(url, browser(),
                                        net::ERR_NAME_NOT_RESOLVED);

  // There should have been two more requests to the correction service:  One
  // for the new error page, and one for tracking purposes.  Have to make sure
  // to wait for the tracking request, since the new error page does not depend
  // on it.
  WaitForRequests(3);
}

// Test that clicking links on a DNS error page works.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_DoClickLink) {
  // The first navigation should fail, and the second one should be the error
  // page.
  GURL url = embedded_test_server()->GetURL("mock.http", "/title2.html");
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
       browser(), GetDnsErrorURL(), 2);
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(1, num_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Simulate a click on a link.

  content::TitleWatcher title_watcher(
      web_contents,
      base::ASCIIToUTF16("Title Of Awesomeness"));
  std::string link_selector =
      "document.querySelector('a[href=\"" + url.spec() + "\"]')";
  // The tracking request is triggered by onmousedown, so it catches middle
  // mouse button clicks, as well as left clicks.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(link_selector + ".onmousedown();"));
  // Can't use content::ExecuteScript because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(link_selector + ".click();"));
  EXPECT_EQ(base::ASCIIToUTF16("Title Of Awesomeness"),
            title_watcher.WaitAndGetTitle());

  // There should have been a tracking request to the correction service.  Have
  // to make sure to wait the tracking request, since the new page does not
  // depend on it.
  WaitForRequests(2);
}

// Test that a DNS error occuring in an iframe does not result in showing
// navigation corrections.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, IFrameDNSError_Basic) {
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/iframe_dns_error.html"), "Blah", 1);
  // We expect to have two history entries, since we started off with navigation
  // to "about:blank" and then navigated to "iframe_dns_error.html".
  EXPECT_EQ(2,
      browser()->tab_strip_model()->GetActiveWebContents()->
          GetController().GetEntryCount());
  EXPECT_EQ(0, num_requests());
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
#if defined(OS_WIN)
#define MAYBE_IFrameDNSError_GoBack DISABLED_IFrameDNSError_GoBack
#else
#define MAYBE_IFrameDNSError_GoBack IFrameDNSError_GoBack
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, MAYBE_IFrameDNSError_GoBack) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  EXPECT_EQ(0, num_requests());
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
//
// This fails on linux_aura bringup: http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
#define MAYBE_IFrameDNSError_GoBackAndForward DISABLED_IFrameDNSError_GoBackAndForward
#else
#define MAYBE_IFrameDNSError_GoBackAndForward IFrameDNSError_GoBackAndForward
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest,
                       MAYBE_IFrameDNSError_GoBackAndForward) {
  NavigateToFileURL("/title2.html");
  NavigateToFileURL("/iframe_dns_error.html");
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  GoForwardAndWaitForTitle("Blah", 1);
  EXPECT_EQ(0, num_requests());
}

// Test that a DNS error occuring in an iframe, once the main document is
// completed loading, does not result in an additional session history entry.
// To ensure that the main document has completed loading, JavaScript is used to
// inject an iframe after loading is done.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, IFrameDNSError_JavaScript) {
  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL fail_url =
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  // Load a regular web page, in which we will inject an iframe.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));

  // We expect to have two history entries, since we started off with navigation
  // to "about:blank" and then navigated to "title2.html".
  EXPECT_EQ(2, wc->GetController().GetEntryCount());

  std::string script = "var frame = document.createElement('iframe');"
                       "frame.src = '" + fail_url.spec() + "';"
                       "document.body.appendChild(frame);";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
    load_observer.Wait();

    // Ensure we saw the expected failure.
    EXPECT_EQ(fail_url, fail_observer.fail_url());

    // Failed initial navigation of an iframe shouldn't be adding any history
    // entries.
    EXPECT_EQ(2, wc->GetController().GetEntryCount());
  }

  // Do the same test, but with an iframe that doesn't have initial URL
  // assigned.
  script = "var frame = document.createElement('iframe');"
           "frame.id = 'target_frame';"
           "document.body.appendChild(frame);";
  {
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
    load_observer.Wait();
  }

  script = "var f = document.getElementById('target_frame');"
           "f.src = '" + fail_url.spec() + "';";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&wc->GetController()));
    wc->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16(script));
    load_observer.Wait();

    EXPECT_EQ(fail_url, fail_observer.fail_url());
    EXPECT_EQ(2, wc->GetController().GetEntryCount());
  }
  EXPECT_EQ(0, num_requests());
}

// Checks that navigation corrections are not loaded when we receive an actual
// 404 page.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Page404) {
  NavigateToURLAndWaitForTitle(embedded_test_server()->GetURL("/page404.html"),
                               "SUCCESS", 1);
  EXPECT_EQ(0, num_requests());
}

// Checks that navigation corrections are loaded in response to a 404 page with
// no body.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Empty404) {
  // The first navigation should fail and load a blank page, while it fetches
  // the Link Doctor response.  The second navigation is the Link Doctor.
  GURL url = embedded_test_server()->GetURL("/errorpage/empty404.html");
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  ExpectDisplayingNavigationCorrections(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), "HTTP ERROR 404");
  EXPECT_EQ(1, num_requests());
}

// Checks that a local error page is shown in response to a 500 error page
// without a body.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Empty500) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/errorpage/empty500.html"));
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), "HTTP ERROR 500");
  EXPECT_EQ(0, num_requests());
}

// Checks that when an error occurs, the stale cache status of the page
// is correctly transferred, and that stale cached copied can be loaded
// from the javascript.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, StaleCacheStatus) {
  // Load cache with entry with "nocache" set, to create stale
  // cache.  Currently it needs to at least have an etag for the cache to
  // not give up on it entirely, however. See https://crbug.com/784520
  GURL test_url(embedded_test_server()->GetURL("/nocache-with-etag.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    partition->GetNetworkContext()->SetFailingHttpTransactionForTesting(
        net::ERR_FAILED);
  }

  // With no navigation corrections to load, there's only one navigation.
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(true));
  EXPECT_TRUE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_NE(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Confirm that loading the stale copy from the cache works.
  content::TestNavigationObserver same_tab_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ReloadStaleCopyFromCache());
  same_tab_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Reload the same URL with a post request; confirm the error page is told
  // that there is no cached copy.
  ui_test_utils::NavigateToURLWithPost(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_EQ(0, num_requests());

  // Clear the cache and reload the same URL; confirm the error page is told
  // that there is no cached copy.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_EQ(0, num_requests());
}

// Check that the easter egg is present and initialised and is not disabled.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, CheckEasterEggIsNotDisabled) {
  ui_test_utils::NavigateToURL(browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check for no disabled message container.
  std::string command = base::StringPrintf(
      "var hasDisableContainer = document.querySelectorAll('.snackbar').length;"
      "domAutomationController.send(hasDisableContainer);");
  int32_t result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
               web_contents, command, &result));
  EXPECT_EQ(0, result);

  // Presence of the canvas container.
  command = base::StringPrintf(
    "var runnerCanvas = document.querySelectorAll('.runner-canvas').length;"
    "domAutomationController.send(runnerCanvas);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
               web_contents, command, &result));
  EXPECT_EQ(1, result);
}

class ErrorPageAutoReloadTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void InstallInterceptor(const GURL& url, int32_t requests_to_fail) {
    requests_ = failures_ = 0;

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](int32_t requests_to_fail, int32_t* requests, int32_t* failures,
               content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.path() == "/searchdomaincheck")
                return false;
              if (params->url_request.url.path() == "/favicon.ico")
                return false;
              if (params->url_request.url.GetOrigin() ==
                  GaiaUrls::GetInstance()->gaia_url())
                return false;
              (*requests)++;
              if (*failures < requests_to_fail) {
                (*failures)++;
                network::URLLoaderCompletionStatus status;
                status.error_code = net::ERR_CONNECTION_RESET;
                params->client->OnComplete(status);
                return true;
              }

              std::string body = URLRequestTestJob::test_data_1();
              content::URLLoaderInterceptor::WriteResponse(
                  URLRequestTestJob::test_headers(), body,
                  params->client.get());
              return true;
            },
            requests_to_fail, &requests_, &failures_));
  }

  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int32_t num_navigations) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  int32_t interceptor_requests() const { return requests_; }
  int32_t interceptor_failures() const { return failures_; }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  int32_t requests_;
  int32_t failures_;
};

// Fails on official mac_trunk build. See crbug.com/465789.
#if defined(OFFICIAL_BUILD) && defined(OS_MACOSX)
#define MAYBE_AutoReload DISABLED_AutoReload
#else
#define MAYBE_AutoReload AutoReload
#endif
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, MAYBE_AutoReload) {
  GURL test_url("http://error.page.auto.reload");
  const int32_t kRequestsToFail = 2;
  InstallInterceptor(test_url, kRequestsToFail);
  NavigateToURLAndWaitForTitle(test_url, "Test One", kRequestsToFail + 1);
  // Note that the interceptor updates these variables on the IO thread,
  // but this function reads them on the main thread. The requests have to be
  // created (on the IO thread) before NavigateToURLAndWaitForTitle returns or
  // this becomes racey.
  EXPECT_EQ(kRequestsToFail, interceptor_failures());
  EXPECT_EQ(kRequestsToFail + 1, interceptor_requests());
}

IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, ManualReloadNotSuppressed) {
  GURL test_url("http://error.page.auto.reload");
  const int32_t kRequestsToFail = 3;
  InstallInterceptor(test_url, kRequestsToFail);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(2, interceptor_requests());

  ToggleHelpBox(browser());
  EXPECT_TRUE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));

  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('reload-button').click();"));
  nav_observer.Wait();
  EXPECT_FALSE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));
}

// Make sure that a same document navigation does not cause issues with the
// auto-reload timer.  Note that this test was added due to this case causing
// a crash.  On regression, this test may hang due to a crashed renderer.
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, IgnoresSameDocumentNavigation) {
  GURL test_url("http://error.page.auto.reload");
  InstallInterceptor(test_url, 2);

  // Wait for the error page and first autoreload, which happens immediately.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(2, interceptor_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.location='#';"));
  content::WaitForLoadStop(web_contents);

  // Same-document navigation on an error page should not have resulted in a
  // new navigation, so no new requests should have been issued.
  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(2, interceptor_requests());

  // Wait for the second auto reload, which succeeds.
  content::TestNavigationObserver observer2(web_contents, 1);
  observer2.Wait();

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(3, interceptor_requests());
}

// A test fixture that returns ERR_ADDRESS_UNREACHABLE for all navigation
// correction requests.  ERR_NAME_NOT_RESOLVED is more typical, but need to use
// a different error for the correction service and the original page to
// validate the right page is being displayed.
class ErrorPageNavigationCorrectionsFailTest : public ErrorPageTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url != google_util::LinkDoctorBaseURL())
                return false;

              network::URLLoaderCompletionStatus status;
              status.error_code = net::ERR_ADDRESS_UNREACHABLE;
              params->client->OnComplete(status);
              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  // Returns true if the platform has support for a diagnostics tool, which
  // can be launched from the error page.
  bool PlatformSupportsDiagnosticsTool() {
#if defined(OS_CHROMEOS)
    // ChromeOS uses an extension instead of a diagnostics dialog.
    return true;
#else
    return CanShowNetworkDiagnosticsDialog();
#endif
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Make sure that when corrections fail to load, the network error page is
// successfully loaded.
IN_PROC_BROWSER_TEST_F(ErrorPageNavigationCorrectionsFailTest,
                       FetchCorrectionsFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      2);

  // Verify that the expected error page is being displayed.
  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_NAME_NOT_RESOLVED);

  // Diagnostics button should be displayed, if available on this platform.
  EXPECT_EQ(PlatformSupportsDiagnosticsTool(),
            IsDisplayingDiagnosticsLink(browser()));
}

// Checks that when an error occurs and a corrections fail to load, the stale
// cache status of the page is correctly transferred, and we can load the
// stale copy from the javascript.  Most logic copied from StaleCacheStatus
// above.
IN_PROC_BROWSER_TEST_F(ErrorPageNavigationCorrectionsFailTest,
                       StaleCacheStatusFailedCorrections) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load cache with entry with "nocache" set, to create stale
  // cache.
  GURL test_url(embedded_test_server()->GetURL("/nocache-with-etag.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    partition->GetNetworkContext()->SetFailingHttpTransactionForTesting(
        net::ERR_CONNECTION_FAILED);
  }

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  EXPECT_TRUE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
  EXPECT_TRUE(ProbeStaleCopyValue(true));

  // Confirm that loading the stale copy from the cache works.
  content::TestNavigationObserver same_tab_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ReloadStaleCopyFromCache());
  same_tab_observer.Wait();
  EXPECT_EQ(base::ASCIIToUTF16("Nocache Test Page"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Clear the cache and reload the same URL; confirm the error page is told
  // that there is no cached copy.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url, 2);
  EXPECT_TRUE(ProbeStaleCopyValue(false));
  EXPECT_FALSE(IsDisplayingText(browser(), GetShowSavedButtonLabel()));
}

class ErrorPageOfflineTest : public ErrorPageTest {
  void SetUpOnMainThread() override {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              return false;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
#if defined(OS_CHROMEOS)
    if (enroll_) {
      // Set up fake install attributes.
      test_install_attributes_ =
          std::make_unique<chromeos::ScopedStubInstallAttributes>(
              chromeos::StubInstallAttributes::CreateCloudManaged("example.com",
                                                                  "fake-id"));
    }
#endif

    // Sets up a mock policy provider for user and device policies.
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::PolicyMap policy_map;
#if defined(OS_CHROMEOS)
    if (enroll_)
      SetEnterpriseUsersDefaults(&policy_map);
#endif
    if (set_allow_dinosaur_easter_egg_) {
      policy_map.Set(
          policy::key::kAllowDinosaurEasterEgg, policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(value_of_allow_dinosaur_easter_egg_),
          nullptr);
    }
    policy_provider_.UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
#else
    policy::ProfilePolicyConnectorFactory::GetInstance()
        ->PushProviderForTesting(&policy_provider_);
#endif

    ErrorPageTest::SetUpInProcessBrowserTestFixture();
  }

  std::string NavigateToPageAndReadText() {
#if defined(OS_CHROMEOS)
    // Check enterprise enrollment
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()
        ->browser_policy_connector_chromeos();
    EXPECT_EQ(enroll_, connector->IsEnterpriseManaged());
#endif

    ui_test_utils::NavigateToURL(
        browser(),
        URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    std::string command = base::StringPrintf(
        "var hasText = document.querySelector('.snackbar');"
        "domAutomationController.send(hasText ? hasText.innerText : '');");

    std::string result;
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, command, &result));

    return result;
  }

  // Whether to set AllowDinosaurEasterEgg policy
  bool set_allow_dinosaur_easter_egg_ = false;

  // The value of AllowDinosaurEasterEgg policy we want to set
  bool value_of_allow_dinosaur_easter_egg_;

#if defined(OS_CHROMEOS)
  // Whether to enroll this CrOS device
  bool enroll_ = true;

  std::unique_ptr<chromeos::ScopedStubInstallAttributes>
      test_install_attributes_;
#endif

  // Mock policy provider for both user and device policies.
  policy::MockConfigurationPolicyProvider policy_provider_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

class ErrorPageOfflineTestWithAllowDinosaurTrue : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = true;
    value_of_allow_dinosaur_easter_egg_ = true;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};

class ErrorPageOfflineTestWithAllowDinosaurFalse : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = true;
    value_of_allow_dinosaur_easter_egg_ = false;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};

#if defined(OS_CHROMEOS)
class ErrorPageOfflineTestUnEnrolledChromeOS : public ErrorPageOfflineTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    set_allow_dinosaur_easter_egg_ = false;
    enroll_ = false;
    ErrorPageOfflineTest::SetUpInProcessBrowserTestFixture();
  }
};
#endif

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurTrue,
                       CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);
}

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurFalse,
                       CheckEasterEggIsDisabled) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ(disabled_text, result);
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTest, CheckEasterEggIsDisabled) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ(disabled_text, result);
}
#else
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTest, CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);
}
#endif

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestUnEnrolledChromeOS,
                       CheckEasterEggIsAllowed) {
  std::string result = NavigateToPageAndReadText();
  std::string disabled_text =
      l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED);
  EXPECT_EQ("", result);
}
#endif

// A test fixture that simulates failing requests for an IDN domain name.
class ErrorPageForIDNTest : public InProcessBrowserTest {
 public:
  // Target hostname in different forms.
  static const char kHostname[];
  static const char kHostnameJSUnicode[];

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(prefs::kAcceptLanguages,
                                                std::string());
  }
};

const char ErrorPageForIDNTest::kHostname[] =
    "xn--d1abbgf6aiiy.xn--p1ai";
const char ErrorPageForIDNTest::kHostnameJSUnicode[] =
    "\\u043f\\u0440\\u0435\\u0437\\u0438\\u0434\\u0435\\u043d\\u0442."
    "\\u0440\\u0444";

// Make sure error page shows correct unicode for IDN.
IN_PROC_BROWSER_TEST_F(ErrorPageForIDNTest, IDN) {
  // ERR_UNSAFE_PORT will not trigger navigation corrections.
  ui_test_utils::NavigateToURL(
      browser(),
      URLRequestFailedJob::GetMockHttpUrlForHostname(net::ERR_UNSAFE_PORT,
                                                     kHostname));
  EXPECT_TRUE(IsDisplayingText(browser(), kHostnameJSUnicode));
}

// Make sure HTTP/0.9 is disabled on non-default ports by default.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Http09WeirdPort) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echo-raw?spam"));
  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_INVALID_HTTP_RESPONSE);
}

// Test that redirects to invalid URLs show an error. See
// https://crbug.com/462272.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, RedirectToInvalidURL) {
  GURL url = embedded_test_server()->GetURL("/server-redirect?https://:");
  ui_test_utils::NavigateToURL(browser(), url);
  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_INVALID_REDIRECT);
  // The error page should commit before the redirect, not after.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

class ErrorPageWithHttp09OnNonDefaultPortsTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PolicyMap values;
    values.Set(policy::key::kHttp09OnNonDefaultPortsEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
};

// Make sure HTTP/0.9 works on non-default ports when enabled by policy.
IN_PROC_BROWSER_TEST_F(ErrorPageWithHttp09OnNonDefaultPortsTest,
                       Http09WeirdPortEnabled) {
  const char kHttp09Response[] = "JumboShrimp";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(std::string("/echo-raw?") +
                                                kHttp09Response));
  EXPECT_TRUE(IsDisplayingText(browser(), kHttp09Response));
}

// Checks that when an HTTP error page is sniffed as a download, an error page
// is displayed. This tests the particular case in which the response body
// is small enough that the entire response must be read before its MIME type
// can be determined.
class ErrorPageSniffTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ErrorPageSniffTest,
                       SniffSmallHttpErrorResponseAsDownload) {
  const char kErrorPath[] = "/foo";
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &DNSErrorPageTest::Return500WithBinaryBody, kErrorPath));
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kErrorPath));

  ExpectDisplayingLocalErrorPage(
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec(),
      browser(), net::ERR_INVALID_RESPONSE);
}

}  // namespace
