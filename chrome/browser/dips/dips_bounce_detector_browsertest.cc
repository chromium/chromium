// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::NavigationHandle;

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host_piece(), url.path_piece()});
}

// Keeps a log of DidStartNavigation, OnCookiesAccessed, and DidFinishNavigation
// executions.
class WCOCallbackLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WCOCallbackLogger> {
 public:
  WCOCallbackLogger(const WCOCallbackLogger&) = delete;
  WCOCallbackLogger& operator=(const WCOCallbackLogger&) = delete;

  const std::vector<std::string>& log() const { return log_; }

 private:
  explicit WCOCallbackLogger(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<WCOCallbackLogger>;

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  std::vector<std::string> log_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WCOCallbackLogger::WCOCallbackLogger(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WCOCallbackLogger>(*web_contents) {}

void WCOCallbackLogger::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  log_.push_back(
      base::StringPrintf("DidStartNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(%s: %s)",
      details.type == content::CookieAccessDetails::Type::kChange ? "Change"
                                                                  : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  log_.push_back(
      base::StringPrintf("DidFinishNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class DIPSBounceDetectorBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    bounce_detector_ =
        DIPSBounceDetector::FromWebContents(GetActiveWebContents());
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DIPSBounceDetector* bounce_detector() { return bounce_detector_; }

 private:
  DIPSBounceDetector* bounce_detector_ = nullptr;
};

// The timing of WCO::OnCookiesAccessed() execution is unpredictable for
// redirects. Sometimes it's called before WCO::DidRedirectNavigation(), and
// sometimes after. Therefore DIPSBounceDetector needs to know when it's safe to
// judge an HTTP redirect as stateful (accessing cookies) or not. This test
// tries to verify that OnCookiesAccessed() is always called before
// DidFinishNavigation(), so that DIPSBounceDetector can safely perform that
// judgement in DidFinishNavigation().
//
// This test also verifies that for redirects that both read and write cookies,
// OnCookiesAccessed() is called with kRead before it's called with kChange.
//
// If either assumption is incorrect, this test will be flaky. On 2022-04-27 I
// (rtarpine) ran this test 1000 times in 40 parallel jobs with no failures, so
// it seems robust.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       AllCookieCallbacksBeforeNavigationFinished) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
      "d.test/set-cookie?name=value");
  GURL final_url =
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on all 4 test domains
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("c.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value")));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit the redirect.
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));

  // Verify that the 7 OnCookiesAccessed() executions are called in order, and
  // all between DidStartNavigation() and DidFinishNavigation().
  //
  // Note: according to web_contents_observer.h, sometimes cookie reads/writes
  // from navigations may cause the RenderFrameHost* overload of
  // OnCookiesAccessed to be called instead. We haven't seen that yet, and this
  // test will intentionally fail if it happens so that we'll notice.
  EXPECT_THAT(
      logger->log(),
      testing::ElementsAre(
          ("DidStartNavigation(a.test/cross-site/b.test/cross-site-with-cookie/"
           "c.test/cross-site-with-cookie/d.test/set-cookie)"),
          ("OnCookiesAccessed(Read: "
           "a.test/cross-site/b.test/cross-site-with-cookie/c.test/"
           "cross-site-with-cookie/d.test/set-cookie)"),
          ("OnCookiesAccessed(Read: "
           "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/d.test/"
           "set-cookie)"),
          ("OnCookiesAccessed(Change: "
           "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/d.test/"
           "set-cookie)"),
          ("OnCookiesAccessed(Read: "
           "c.test/cross-site-with-cookie/d.test/set-cookie)"),
          ("OnCookiesAccessed(Change: "
           "c.test/cross-site-with-cookie/d.test/set-cookie)"),
          "OnCookiesAccessed(Read: d.test/set-cookie)",
          "OnCookiesAccessed(Change: d.test/set-cookie)",
          "DidFinishNavigation(d.test/set-cookie)"));
}

void AppendRedirectURL(std::vector<std::string>* urls,
                       const GURL& prev_url,
                       NavigationHandle* navigation_handle,
                       int i,
                       CookieAccessType access) {
  if (access != CookieAccessType::kNone)
    urls->push_back(
        base::StrCat({FormatURL(navigation_handle->GetRedirectChain()[i]), " (",
                      CookieAccessTypeToString(access), ")"}));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulServerRedirect) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/d.test/"
      "title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on a.test, b.test and d.test (but not c.test).
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value")));

  std::vector<std::string> stateful_redirects;
  bounce_detector()->SetStatefulServerRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirectURL, &stateful_redirects));

  // Visit the redirect.
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));

  // a.test and b.test are stateful redirects. c.test had no cookies, and d.test
  // was not a redirect.
  EXPECT_THAT(
      stateful_redirects,
      testing::ElementsAre(
          ("a.test/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/"
           "d.test/title1.html (ReadWrite)"),
          ("b.test/cross-site/c.test/cross-site/d.test/title1.html (Read)")));
}

// An EmbeddedTestServer request handler for
// /cross-site-with-samesite-none-cookie URLs. Like /cross-site-with-cookie, but
// the cookie has additional Secure and SameSite=None attributes.
std::unique_ptr<net::test_server::HttpResponse>
HandleCrossSiteSameSiteNoneCookieRedirect(
    net::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  const std::string prefix = "/cross-site-with-samesite-none-cookie";
  if (!net::test_server::ShouldHandle(request, prefix))
    return nullptr;

  std::string dest_all = base::UnescapeBinaryURLComponent(
      request.relative_url.substr(prefix.size() + 1));

  std::string dest;
  size_t delimiter = dest_all.find("/");
  if (delimiter != std::string::npos) {
    dest = base::StringPrintf(
        "//%s:%hu/%s", dest_all.substr(0, delimiter).c_str(), server->port(),
        dest_all.substr(delimiter + 1).c_str());
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie",
                                 "server-redirect=true; Secure; SameSite=None");
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      dest.c_str()));
  return http_response;
}

// Ignore iframes because their state will be partitioned under the top-level
// site anyway.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       IgnoreServerRedirectsInIframes) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  const GURL root_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const GURL redirect_url = https_server.GetURL(
      "b.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html");
  const std::string iframe_id = "test";
  content::WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> stateful_redirects;
  bounce_detector()->SetStatefulServerRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirectURL, &stateful_redirects));

  ASSERT_TRUE(content::NavigateToURL(web_contents, root_url));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, iframe_id, redirect_url));

  // b.test had a stateful redirect, but because it was in an iframe, we ignored
  // it.
  EXPECT_THAT(stateful_redirects, testing::IsEmpty());
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const GURL& prev_url,
                    const GURL& url,
                    const GURL& next_url,
                    CookieAccessType access) {
  if (access != CookieAccessType::kNone)
    redirects->push_back(
        base::StrCat({FormatURL(prev_url), " -> ", FormatURL(url), " -> ",
                      FormatURL(next_url)}));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Server) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/d.test/"
      "title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on a.test, b.test and d.test (but not c.test).
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value")));

  std::vector<std::string> redirects;
  bounce_detector()->SetStatefulRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Visit the redirect.
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));

  // a.test and b.test are stateful redirects. c.test had no cookies, and d.test
  // was not a redirect.
  EXPECT_THAT(redirects,
              testing::ElementsAre(
                  ("a.test/title1.html -> "
                   "a.test/cross-site-with-cookie/b.test/cross-site/c.test/"
                   "cross-site/d.test/title1.html -> d.test/title1.html"),
                  ("a.test/title1.html -> "
                   "b.test/cross-site/c.test/cross-site/d.test/title1.html -> "
                   "d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_NoContent) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  // The redirect chain ends in a 204 No Content response, which doesn't commit.
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/d.test/"
      "nocontent");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on a.test, b.test and d.test (but not c.test).
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value")));

  std::vector<std::string> redirects;
  bounce_detector()->SetStatefulRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Visit the redirect (note that the user ends up back at initial_url).
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url,
                                     /*expected_commit_url=*/initial_url));

  // a.test and b.test are stateful redirects. c.test had no cookies, and d.test
  // was not a redirect.
  EXPECT_THAT(redirects,
              testing::ElementsAre(
                  ("a.test/title1.html -> "
                   "a.test/cross-site-with-cookie/b.test/cross-site/c.test/"
                   "cross-site/d.test/nocontent -> d.test/nocontent"),
                  ("a.test/title1.html -> "
                   "b.test/cross-site/c.test/cross-site/d.test/nocontent -> "
                   "d.test/nocontent")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_404Error) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  // The redirect chain ends in a 404 error.
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/d.test/404");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on a.test, b.test and d.test (but not c.test).
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value")));

  std::vector<std::string> redirects;
  bounce_detector()->SetStatefulRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Visit the redirect, ending up on an error page.
  ASSERT_FALSE(content::NavigateToURL(web_contents, redirect_url));
  ASSERT_TRUE(content::IsLastCommittedEntryOfPageType(
      web_contents, content::PAGE_TYPE_ERROR));

  // a.test and b.test are stateful redirects. c.test had no cookies, and d.test
  // was not a redirect.
  EXPECT_THAT(redirects,
              testing::ElementsAre(
                  ("a.test/title1.html -> "
                   "a.test/cross-site-with-cookie/b.test/cross-site/c.test/"
                   "cross-site/d.test/404 -> d.test/404"),
                  ("a.test/title1.html -> "
                   "b.test/cross-site/c.test/cross-site/d.test/404 -> "
                   "d.test/404")));
}

using base::Bucket;

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Histograms_BounceCategory) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/d.test/"
      "title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on a.test and b.test. Note that browser-initiated navigations
  // like these are treated as a sign of user engagement.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/set-cookie?name=value")));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));
  // Navigate to starting page.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, initial_url));

  // Visit the redirect and monitor the histograms.
  base::HistogramTester histograms;
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, redirect_url,
                                                 final_url));

  // Verify the correct histogram was used for all samples.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategory.Standard"] = 2;
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategory."),
      testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. Note that the a.test redirect was
  // not reported because the previously committed page was also on a.test.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategory.Standard"),
      testing::ElementsAre(
          // c.test
          Bucket((int)RedirectCategory::kNoCookies_NoEngagement, 1),
          // b.test
          Bucket((int)RedirectCategory::kReadCookies_HasEngagement, 1)));
}
