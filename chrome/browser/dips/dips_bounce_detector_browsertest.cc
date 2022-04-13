// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
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
  log_.push_back(base::StringPrintf("OnCookiesAccessed(%s)",
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
// judgement in DidFinishNavigation(). If the assumption is incorrect, this test
// will be flaky. On 2022-04-13 I (rtarpine) ran this test 1000 times in 40
// parallel jobs with no failures, so it seems robust.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       AllCookieCallbacksBeforeNavigationFinished) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site/c.test/cross-site/d.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
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

  // Verify that the 4 OnCookiesAccessed() executions are called in order,
  // and all between DidStartNavigation() and DidFinishNavigation().
  EXPECT_THAT(logger->log(),
              testing::ElementsAre(
                  "DidStartNavigation(a.test/cross-site/b.test/cross-site/"
                  "c.test/cross-site/d.test/title1.html)",
                  "OnCookiesAccessed(a.test/cross-site/b.test/cross-site/"
                  "c.test/cross-site/d.test/title1.html)",
                  "OnCookiesAccessed(b.test/cross-site/c.test/cross-site/"
                  "d.test/title1.html)",
                  "OnCookiesAccessed(c.test/cross-site/d.test/title1.html)",
                  "OnCookiesAccessed(d.test/title1.html)",
                  "DidFinishNavigation(d.test/title1.html)"));
}

void AppendRedirectURL(std::vector<std::string>* urls,
                       NavigationHandle* navigation_handle,
                       int i) {
  urls->push_back(FormatURL(navigation_handle->GetRedirectChain()[i]));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest, DetectStatefulBounces) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site/c.test/cross-site/d.test/title1.html");
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
  bounce_detector()->SetStatefulRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirectURL, &stateful_redirects));

  // Visit the redirect.
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));

  // a.test and b.test are stateful redirects. c.test had no cookies, and d.test
  // was not a redirect.
  EXPECT_THAT(stateful_redirects,
              testing::ElementsAre(
                  "a.test/cross-site/b.test/cross-site/c.test/cross-site/"
                  "d.test/title1.html",
                  "b.test/cross-site/c.test/cross-site/d.test/title1.html"));
}
