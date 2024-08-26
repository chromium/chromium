// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"  // nogncheck
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::BrowserThread;
using content::NavigationController;
using net::URLRequestFailedJob;
using net::URLRequestTestJob;

namespace {

// Searches for first node containing |text|, and if it finds one, searches
// through all ancestors seeing if any of them is of class "hidden". Since it
// relies on the hidden class used by network error pages, not suitable for
// general use.
[[nodiscard]] bool IsDisplayingText(content::RenderFrameHost* render_frame_host,
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
    isNodeVisible(node);
  )", text.c_str());
  // clang-format on
  return content::EvalJs(render_frame_host, command).ExtractBool();
}

[[nodiscard]] bool IsDisplayingText(Browser* browser, const std::string& text) {
  return IsDisplayingText(
      browser->tab_strip_model()->GetActiveWebContents()->GetPrimaryMainFrame(),
      text);
}

// Expands the more box on the currently displayed error page.
void ToggleHelpBox(Browser* browser) {
  EXPECT_TRUE(
      content::ExecJs(browser->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('details-button').click();"));
}

// Returns true if the diagnostics link suggestion is displayed.
[[nodiscard]] bool IsDisplayingDiagnosticsLink(Browser* browser) {
  std::string command = base::StringPrintf(
      "var diagnose_link = document.getElementById('diagnose-link');"
      "diagnose_link != null;");
  return content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                         command)
      .ExtractBool();
}

// Checks that the error page is being displayed with the specified error
// string.
void ExpectDisplayingErrorPage(Browser* browser,
                               const std::string& error_string) {
  EXPECT_TRUE(IsDisplayingText(browser, error_string));
}

// Checks that the error page is being displayed with the specified error code.
void ExpectDisplayingErrorPage(Browser* browser, net::Error error_code) {
  ExpectDisplayingErrorPage(browser, net::ErrorToShortString(error_code));
}

// Returns true if the platform has support for a diagnostics tool, and it
// can be launched from |web_contents|.
bool WebContentsCanShowDiagnosticsTool(content::WebContents* web_contents) {
  return CanShowNetworkDiagnosticsDialog(web_contents);
}

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  ErrorPageTest() = default;
  ~ErrorPageTest() override = default;

  // Navigates the active tab to a mock url created for the file at |path|.
  void NavigateToFileURL(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  // Navigates to the given URL and waits for the title to change to
  // |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  // Navigates back in the history and waits for the title to change to
  // |expected_title|.
  void GoBackAndWaitForTitle(const std::string& expected_title) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   HISTORY_NAVIGATE_BACK);
  }

  // Navigates forward in the history and waits for  the title to change to
  // |expected_title|.
  void GoForwardAndWaitForTitle(const std::string& expected_title) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   HISTORY_NAVIGATE_FORWARD);
  }

  void GoBackAndWaitForNavigations() { NavigateHistory(HISTORY_NAVIGATE_BACK); }

  void GoForwardAndWaitForNavigations() {
    NavigateHistory(HISTORY_NAVIGATE_FORWARD);
  }

  // Navigates the browser the indicated direction in the history and waits for
  // |num_navigations| to occur and the title to change to |expected_title|.
  void NavigateHistoryAndWaitForTitle(const std::string& expected_title,
                                      HistoryNavigationDirection direction) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    NavigateHistory(direction);

    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              base::ASCIIToUTF16(expected_title));
  }

  void NavigateHistory(HistoryNavigationDirection direction) {
    content::TestNavigationObserver test_navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), 1);
    if (direction == HISTORY_NAVIGATE_BACK) {
      chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
    } else {
      FAIL();
    }
    test_navigation_observer.Wait();
  }
};

class TestFailProvisionalLoadObserver : public content::WebContentsObserver {
 public:
  explicit TestFailProvisionalLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  TestFailProvisionalLoadObserver(const TestFailProvisionalLoadObserver&) =
      delete;
  TestFailProvisionalLoadObserver& operator=(
      const TestFailProvisionalLoadObserver&) = delete;

  ~TestFailProvisionalLoadObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsErrorPage())
      fail_url_ = navigation_handle->GetURL();
  }

  const GURL& fail_url() const { return fail_url_; }

 private:
  GURL fail_url_;
};

class DNSErrorPageTest : public ErrorPageTest {
 public:
  DNSErrorPageTest() {
    // Inject a URLLoaderInterceptor. While the injected callback doesn't
    // intercept any URLs itself, URLLoaderInterceptor automatically intercepts
    // URLRequestFailedJob URLs, which these tests used to simulate network
    // errors.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              return false;
            }));
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
  }

  // Returns a GURL that results in a DNS error.
  GURL GetDnsErrorURL() const {
    return URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  }

 private:
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_existent_file_url));

  ExpectDisplayingErrorPage(browser(), net::ERR_FILE_NOT_FOUND);
  // Only errors on HTTP/HTTPS pages should display a diagnostics button.
  EXPECT_FALSE(IsDisplayingDiagnosticsLink(browser()));
}

// Test that a DNS error occurring in the main frame displays an error page.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_Basic) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  // Diagnostics button should be displayed, if available.
  EXPECT_EQ(WebContentsCanShowDiagnosticsTool(
                browser()->tab_strip_model()->GetActiveWebContents()),
            IsDisplayingDiagnosticsLink(browser()));
}

// Test that a DNS error occurring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack1) {
  NavigateToFileURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
  GoBackAndWaitForTitle("Title Of Awesomeness");
}

// Test that a DNS error occurring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL("/title2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  NavigateToFileURL("/title3.html");

  GoBackAndWaitForNavigations();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
}

// Test that a DNS error occurring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL("/title2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));

  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  NavigateToFileURL("/title3.html");

  GoBackAndWaitForNavigations();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  GoBackAndWaitForTitle("Title Of Awesomeness");

  GoForwardAndWaitForNavigations();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
}

// Test that a DNS error occurring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_GoBack2Forward2) {
  NavigateToFileURL("/title3.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  NavigateToFileURL("/title2.html");

  GoBackAndWaitForNavigations();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  GoBackAndWaitForTitle("Title Of More Awesomeness");

  GoForwardAndWaitForNavigations();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  GoForwardAndWaitForTitle("Title Of Awesomeness");
}

// Test that the reload button on a DNS error page works.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, DNSError_DoReload) {
  // The first navigation should fail, and the second one should be the error
  // page.
  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Clicking the reload button should load the error page again.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  // Can't use content::ExecJs because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.getElementById('reload-button').click();",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  nav_observer.Wait();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
}

// Test that the reload button on a DNS error page works after a same document
// navigation on the error page.  Error pages don't seem to do this, but some
// traces indicate this may actually happen.  This test may hang on regression.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest,
                       DNSError_DoReloadAfterSameDocumentNavigation) {
  std::string url =
      embedded_test_server()->GetURL("mock.http", "/title2.html").spec();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetDnsErrorURL()));
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Do a same-document navigation on the error page, which should not result
  // in a new navigation.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.location='#';", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  content::WaitForLoadStop(web_contents);
  // Page being displayed should not change.
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);

  // Clicking the reload button should load the error page again.
  content::TestNavigationObserver nav_observer2(web_contents);
  // Can't use content::ExecJs because it waits for scripts to send
  // notification that they've run, and scripts that trigger a navigation may
  // not send that notification.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.getElementById('reload-button').click();",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  nav_observer2.Wait();
  ExpectDisplayingErrorPage(browser(), net::ERR_NAME_NOT_RESOLVED);
}

// Test a DNS error occurring in an iframe.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, IFrameDNSError) {
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/iframe_dns_error.html"), "Blah");

  // There should be a child iframe with a DNS error.
  content::RenderFrameHost* child_frame =
      ChildFrameAt(browser()->tab_strip_model()->GetActiveWebContents(), 0);
  ASSERT_TRUE(child_frame);

  EXPECT_TRUE(IsDisplayingText(
      child_frame, net::ErrorToShortString(net::ERR_NAME_NOT_RESOLVED)));
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
#if BUILDFLAG(IS_WIN)
#define MAYBE_IFrameDNSError_GoBack DISABLED_IFrameDNSError_GoBack
#else
#define MAYBE_IFrameDNSError_GoBack IFrameDNSError_GoBack
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, MAYBE_IFrameDNSError_GoBack) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title2.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_dns_error.html")));
  GoBackAndWaitForTitle("Title Of Awesomeness");
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
//
// This fails on linux_aura bringup: http://crbug.com/163931
#if BUILDFLAG(IS_WIN) ||                                       \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
     defined(USE_AURA))
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
  GoBackAndWaitForTitle("Title Of Awesomeness");
  GoForwardAndWaitForTitle("Blah");
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title2.html")));

  // We expect to have two history entries, since we started off with navigation
  // to "about:blank" and then navigated to "title2.html".
  EXPECT_EQ(2, wc->GetController().GetEntryCount());

  std::string script = "var frame = document.createElement('iframe');"
                       "frame.src = '" + fail_url.spec() + "';"
                       "document.body.appendChild(frame);";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::LoadStopObserver load_observer(wc);
    wc->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script), base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
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
    content::LoadStopObserver load_observer(wc);
    wc->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script), base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    load_observer.Wait();
  }

  script = "var f = document.getElementById('target_frame');"
           "f.src = '" + fail_url.spec() + "';";
  {
    TestFailProvisionalLoadObserver fail_observer(wc);
    content::LoadStopObserver load_observer(wc);
    wc->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script), base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    load_observer.Wait();

    EXPECT_EQ(fail_url, fail_observer.fail_url());
    EXPECT_EQ(2, wc->GetController().GetEntryCount());
  }
}

// Checks that the error page is not displayed when receiving an actual 404
// page.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Page404) {
  NavigateToURLAndWaitForTitle(embedded_test_server()->GetURL("/page404.html"),
                               "SUCCESS");
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  EXPECT_FALSE(IsDisplayingText(browser(), "HTTP ERROR 404"));
}

// Checks that a local error page is shown in response to a 404 error page
// without a body.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Empty404) {
  GURL url = embedded_test_server()->GetURL("/errorpage/empty404.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // This depends on the non-internationalized error ID string in
  // localized_error.cc.
  ExpectDisplayingErrorPage(browser(), "HTTP ERROR 404");
}

// Check that the easter egg is present and initialised.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, CheckEasterEgg) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED)));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check for no disabled message container.
  std::string command = base::StringPrintf(
      "var hasDisableContainer = document.querySelectorAll('.snackbar').length;"
      "hasDisableContainer;");
  EXPECT_EQ(0, content::EvalJs(web_contents, command));

  // Presence of the canvas container.
  command = base::StringPrintf(
      "var runnerCanvas = document.querySelectorAll('.runner-canvas').length;"
      "runnerCanvas;");
  EXPECT_EQ(1, content::EvalJs(web_contents, command));
}

// Test error page in incognito mode. The only difference is that no network
// diagnostic link is included, except on ChromeOS.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Incognito) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser,
      URLRequestFailedJob::GetMockHttpsUrl(net::ERR_NAME_NOT_RESOLVED)));

  // Verify that the expected error page is being displayed.
  ExpectDisplayingErrorPage(incognito_browser, net::ERR_NAME_NOT_RESOLVED);

#if !BUILDFLAG(IS_CHROMEOS)
  // Can't currently show the diagnostics in incognito on any platform but
  // ChromeOS.
  EXPECT_FALSE(WebContentsCanShowDiagnosticsTool(
      incognito_browser->tab_strip_model()->GetActiveWebContents()));
#endif

  // Diagnostics button should be displayed, if available.
  EXPECT_EQ(WebContentsCanShowDiagnosticsTool(
                incognito_browser->tab_strip_model()->GetActiveWebContents()),
            IsDisplayingDiagnosticsLink(incognito_browser));
}

class ErrorPageAutoReloadTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(embedder_support::kEnableAutoReload);
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void InstallInterceptor(const GURL& url, int32_t requests_to_fail) {
    requests_ = failures_ = 0;

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](int32_t requests_to_fail, int32_t* requests, int32_t* failures,
               content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host().find("googleapis.com") !=
                  std::string::npos) {
                return false;
              }
              if (params->url_request.url.path() == "/searchdomaincheck")
                return false;
              if (params->url_request.url.path() == "/favicon.ico")
                return false;
              if (params->url_request.url.DeprecatedGetOriginAsURL() ==
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
                                    const std::string& expected_title) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16(expected_title));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  void NavigateAndWaitForFailureWithAutoReload(const GURL& url) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Expect the first navigation to fail with a committed error page.
    content::TestNavigationManager first_navigation(web_contents, url);
    web_contents->GetController().LoadURL(url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          /*extra_headers=*/std::string());
    ASSERT_TRUE(first_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(first_navigation.was_committed());
    EXPECT_FALSE(first_navigation.was_successful());

    // Expect a second navigation to result from a failed auto-reload attempt.
    // This should not be committed.
    content::TestNavigationManager failed_auto_reload_navigation(web_contents,
                                                                 url);
    ASSERT_TRUE(failed_auto_reload_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(failed_auto_reload_navigation.was_committed());
  }

  int32_t interceptor_requests() const { return requests_; }
  int32_t interceptor_failures() const { return failures_; }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  int32_t requests_;
  int32_t failures_;
};

// Fails on official mac_trunk build. See crbug.com/465789.
#if defined(OFFICIAL_BUILD) && BUILDFLAG(IS_MAC)
#define MAYBE_AutoReload DISABLED_AutoReload
#else
#define MAYBE_AutoReload AutoReload
#endif
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest, MAYBE_AutoReload) {
  GURL test_url("http://error.page.auto.reload");
  const int32_t kRequestsToFail = 2;
  InstallInterceptor(test_url, kRequestsToFail);
  NavigateToURLAndWaitForTitle(test_url, "Test One");
  // Note that the interceptor updates these variables on the IO thread,
  // but this function reads them on the main thread. The requests have to be
  // created (on the IO thread) before NavigateToURLAndWaitForTitle returns or
  // this becomes racey.
  EXPECT_EQ(kRequestsToFail, interceptor_failures());
  EXPECT_EQ(kRequestsToFail + 1, interceptor_requests());
}

// TODO(crbug.com/40856405): Test is flaky.
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest,
                       DISABLED_ManualReloadNotSuppressed) {
  GURL test_url("http://error.page.auto.reload");
  const int32_t kRequestsToFail = 3;
  InstallInterceptor(test_url, kRequestsToFail);

  // Wait for the error page and first autoreload.
  NavigateAndWaitForFailureWithAutoReload(test_url);

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(2, interceptor_requests());

  ToggleHelpBox(browser());
  EXPECT_TRUE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));

  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.getElementById('reload-button').click();",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  nav_observer.Wait();
  EXPECT_FALSE(IsDisplayingText(
      browser(), l10n_util::GetStringUTF8(
                     IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER)));
}

// Make sure that a same document navigation does not cause issues with the
// auto-reload timer.  Note that this test was added due to this case causing
// a crash.  On regression, this test may hang due to a crashed renderer.
// TODO(crbug.com/40709227): Flaky.
IN_PROC_BROWSER_TEST_F(ErrorPageAutoReloadTest,
                       DISABLED_IgnoresSameDocumentNavigation) {
  GURL test_url("http://error.page.auto.reload");
  InstallInterceptor(test_url, 2);

  // Wait for the error page and first autoreload.
  NavigateAndWaitForFailureWithAutoReload(test_url);

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(2, interceptor_requests());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::u16string kExpectedTitle = u"Test One";
  content::TitleWatcher title_watcher(web_contents, kExpectedTitle);

  // Same-document navigation on an error page should not interrupt the
  // scheduled auto-reload which should still be pending on the WebContents.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.location='#';", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);

  // Wait for the second auto reload to happen. It will succeed and update the
  // WebContents' title.
  EXPECT_EQ(kExpectedTitle, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(2, interceptor_failures());
  EXPECT_EQ(3, interceptor_requests());
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
    // Sets up a mock policy provider for user and device policies.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::PolicyMap policy_map;
    if (set_allow_dinosaur_easter_egg_) {
      policy_map.Set(policy::key::kAllowDinosaurEasterEgg,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     base::Value(value_of_allow_dinosaur_easter_egg_), nullptr);
    }

#if BUILDFLAG(IS_CHROMEOS)
    SetEnterpriseUsersProfileDefaults(&policy_map);
#endif

    policy_provider_.UpdateChromePolicy(policy_map);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);
    ErrorPageTest::SetUpInProcessBrowserTestFixture();
  }

  std::string NavigateToPageAndReadText() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED)));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    std::string command = base::StringPrintf(
        "var hasText = document.querySelector('.snackbar');"
        "hasText ? hasText.innerText : '';");

    return content::EvalJs(web_contents, command).ExtractString();
  }

  // Whether to set AllowDinosaurEasterEgg policy
  bool set_allow_dinosaur_easter_egg_ = false;

  // The value of AllowDinosaurEasterEgg policy we want to set
  bool value_of_allow_dinosaur_easter_egg_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
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

#if BUILDFLAG(IS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurTrue,
                       CheckEasterEggHighScoreLoaded) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  IntegerPrefMember easter_egg_high_score;
  easter_egg_high_score.Init(prefs::kNetworkEasterEggHighScore,
                             profile->GetPrefs());

  // Set a high score in the user's profile.
  int high_score = 1000;
  easter_egg_high_score.SetValue(high_score);

  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);

  content::EvalJsResult actual_high_score = content::EvalJs(
      web_contents,
      "new Promise((resolve) => {"
      "  window.initializeEasterEggHighScore = function(highscore) { "
      "    resolve(highscore);"
      "  };"
      "  /* Request the initial highscore from the browser. */"
      "  errorPageController.trackEasterEgg();"
      "});");

  EXPECT_EQ(high_score, actual_high_score);
}

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineTestWithAllowDinosaurTrue,
                       CheckEasterEggHighScoreSaved) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  IntegerPrefMember easter_egg_high_score;
  easter_egg_high_score.Init(prefs::kNetworkEasterEggHighScore,
                             profile->GetPrefs());

  // The high score should be initialized to 0.
  EXPECT_EQ(0, easter_egg_high_score.GetValue());

  std::string result = NavigateToPageAndReadText();
  EXPECT_EQ("", result);

  {
    base::RunLoop run_loop;
    PrefChangeRegistrar change_observer;
    change_observer.Init(profile->GetPrefs());
    change_observer.Add(prefs::kNetworkEasterEggHighScore,
                        run_loop.QuitClosure());

    // Save a new high score.
    EXPECT_TRUE(content::ExecJs(
        web_contents, "errorPageController.updateEasterEggHighScore(2000);"));

    // Wait for preference change.
    run_loop.Run();
    EXPECT_EQ(2000, easter_egg_high_score.GetValue());
  }

  {
    base::RunLoop run_loop;
    PrefChangeRegistrar change_observer;
    change_observer.Init(profile->GetPrefs());
    change_observer.Add(prefs::kNetworkEasterEggHighScore,
                        run_loop.QuitClosure());

    // Reset high score back to 0.
    EXPECT_TRUE(content::ExecJs(
        web_contents, "errorPageController.resetEasterEggHighScore();"));

    // Wait for preference change.
    run_loop.Run();
    EXPECT_EQ(0, easter_egg_high_score.GetValue());
  }
}

// A test fixture that simulates failing requests for an IDN domain name.
class ErrorPageForIDNTest : public InProcessBrowserTest {
 public:
  // Target hostname in different forms.
  static const char kHostname[];
  static const char kHostnameJSUnicode[];

  ErrorPageForIDNTest() {
    // TODO(crbug.com/334954143) This test clears the AcceptLanguage Prefs which
    // causes Accept-Language to not work correctly. Fix the tests when turning
    // on the reduce accept-language feature.
    scoped_feature_list_.InitWithFeatures(
        {}, {network::features::kReduceAcceptLanguage});
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(
        language::prefs::kAcceptLanguages, std::string());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

const char ErrorPageForIDNTest::kHostname[] =
    "xn--d1abbgf6aiiy.xn--p1ai";
const char ErrorPageForIDNTest::kHostnameJSUnicode[] =
    "\\u043f\\u0440\\u0435\\u0437\\u0438\\u0434\\u0435\\u043d\\u0442."
    "\\u0440\\u0444";

// Make sure error page shows correct unicode for IDN.
IN_PROC_BROWSER_TEST_F(ErrorPageForIDNTest, IDN) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), URLRequestFailedJob::GetMockHttpUrlForHostname(
                     net::ERR_UNSAFE_PORT, kHostname)));
  EXPECT_TRUE(IsDisplayingText(browser(), kHostnameJSUnicode));
}

// Make sure HTTP/0.9 is disabled on non-default ports by default.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, Http09WeirdPort) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echo-raw?spam")));
  ExpectDisplayingErrorPage(browser(), net::ERR_INVALID_HTTP_RESPONSE);
}

// Test that redirects to invalid URLs show an error. See
// https://crbug.com/462272.
IN_PROC_BROWSER_TEST_F(DNSErrorPageTest, RedirectToInvalidURL) {
  GURL url = embedded_test_server()->GetURL("/server-redirect?https://:");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ExpectDisplayingErrorPage(browser(), net::ERR_INVALID_REDIRECT);
  // The error page should commit before the redirect, not after.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

// Checks that when an HTTP error page is sniffed as a download, an error page
// is displayed. This tests the particular case in which the response body
// is small enough that the entire response must be read before its MIME type
// can be determined.
using ErrorPageSniffTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ErrorPageSniffTest,
                       SniffSmallHttpErrorResponseAsDownload) {
  const char kErrorPath[] = "/foo";
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &DNSErrorPageTest::Return500WithBinaryBody, kErrorPath));
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kErrorPath)));

  ExpectDisplayingErrorPage(browser(), net::ERR_INVALID_RESPONSE);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// For ChromeOS, launches appropriate diagnostics app.
void ClickDiagnosticsLink(Browser* browser) {
  DCHECK(IsDisplayingDiagnosticsLink(browser));
  EXPECT_TRUE(
      content::ExecJs(browser->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('diagnose-link').click();"));
}

// On ChromeOS "Running Connectivity Diagnostics" link on error page should
// launch chrome://diagnostics/?connectivity app by default. Not running test on
// LaCROS due to errors on Wayland initialization and to keep test to ChromeOS
// devices.
using ErrorPageOfflineAppLaunchTest = ash::SystemWebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(ErrorPageOfflineAppLaunchTest, DiagnosticsConnectivity) {
  WaitForTestSystemAppInstall();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_INTERNET_DISCONNECTED)));

  const GURL expected_url = GURL("chrome://diagnostics/?connectivity");
  content::TestNavigationObserver observer(expected_url);
  observer.StartWatchingNewWebContents();

  // Click to open diagnostics app.
  ClickDiagnosticsLink(browser());
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // The active screen should be Connectivity Diagnostics app.
  content::WebContents* contents =
      ::chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
