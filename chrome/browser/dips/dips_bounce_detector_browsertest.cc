// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_bounce_detector.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-shared.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

using base::Bucket;
using blink::mojom::EngagementLevel;
using content::NavigationHandle;
using content::WebContents;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::Pair;
using ukm::builders::DIPS_Redirect;

namespace {

class UserActivationObserver : public content::WebContentsObserver {
 public:
  explicit UserActivationObserver(content::WebContents* web_contents,
                                  content::RenderFrameHost* render_frame_host)
      : WebContentsObserver(web_contents),
        render_frame_host_(render_frame_host) {}

  // Wait until the frame receives user activation.
  void Wait() { run_loop_.Run(); }

  // WebContentsObserver override
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ == render_frame_host) {
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<content::RenderFrameHost> const render_frame_host_;
  base::RunLoop run_loop_;
};

class CookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit CookieAccessObserver(content::WebContents* web_contents,
                                content::RenderFrameHost* render_frame_host)
      : WebContentsObserver(web_contents),
        render_frame_host_(render_frame_host) {}

  // Wait until the frame accesses cookies.
  void Wait() { run_loop_.Run(); }

  // WebContentsObserver override
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override {
    if (render_frame_host_ == render_frame_host) {
      run_loop_.Quit();
    }
  }

 private:
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  base::RunLoop run_loop_;
};

}  // namespace

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
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
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
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Callbacks for favicons are ignored only in testing logs because their
  // ordering is variable and would cause flakiness
  if (!render_frame_host->IsInPrimaryMainFrame() ||
      FormatURL(details.url).find("favicon.ico") != std::string::npos) {
    return;
  }

  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(RenderFrameHost, %s: %s)",
      details.type == content::CookieAccessDetails::Type::kChange ? "Change"
                                                                  : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(NavigationHandle, %s: %s)",
      details.type == content::CookieAccessDetails::Type::kChange ? "Change"
                                                                  : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Android testing produces callbacks for a finished navigation to "blank" at
  // the beginning of a test. These should be ignored here.
  if (FormatURL(navigation_handle->GetURL()) == "blank" ||
      navigation_handle->GetPreviousPrimaryMainFrameURL().is_empty()) {
    return;
  }
  log_.push_back(
      base::StringPrintf("DidFinishNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class DIPSBounceDetectorBrowserTest : public PlatformBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DIPSBounceDetector::SetTickClockForTesting(&test_clock_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("sub.b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    bounce_detector_ =
        DIPSBounceDetector::FromWebContents(GetActiveWebContents());
  }

  void TearDownInProcessBrowserTestFixture() override {
    DIPSBounceDetector::SetTickClockForTesting(nullptr);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  DIPSBounceDetector* bounce_detector() { return bounce_detector_; }

  void SetDIPSTime(base::TimeTicks ticks) { test_clock_.SetNowTicks(ticks); }

  void AdvanceDIPSTime(base::TimeDelta delta) { test_clock_.Advance(delta); }

  void CreateImageAndWaitForCookieAccess(const GURL& image_url) {
    WebContents* web_contents = GetActiveWebContents();
    CookieAccessObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(content::ExecJs(web_contents,
                                content::JsReplace(
                                    R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                    image_url),
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    // The image must cause a cookie access, or else this will hang.
    observer.Wait();
  }

 private:
  base::SimpleTestTickClock test_clock_;
  raw_ptr<DIPSBounceDetector> bounce_detector_ = nullptr;
};

// The timing of WCO::OnCookiesAccessed() execution is unpredictable for
// redirects. Sometimes it's called before WCO::DidRedirectNavigation(), and
// sometimes after. Therefore DIPSBounceDetector needs to know when it's safe to
// judge an HTTP redirect as stateful (accessing cookies) or not. This test
// tries to verify that OnCookiesAccessed() is always called before
// DidFinishNavigation(), so that DIPSBounceDetector can safely perform that
// judgement in DidFinishNavigation().
//
// This test also verifies that OnCookiesAccessed() is called for URLs in the
// same order that they're visited (and that for redirects that both read and
// write cookies, OnCookiesAccessed() is called with kRead before it's called
// with kChange, although DIPSBounceDetector doesn't depend on that anymore.)
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
          ("OnCookiesAccessed(NavigationHandle, Read: "
           "a.test/cross-site/b.test/cross-site-with-cookie/c.test/"
           "cross-site-with-cookie/d.test/set-cookie)"),
          ("OnCookiesAccessed(NavigationHandle, Read: "
           "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/d.test/"
           "set-cookie)"),
          ("OnCookiesAccessed(NavigationHandle, Change: "
           "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/d.test/"
           "set-cookie)"),
          ("OnCookiesAccessed(NavigationHandle, Read: "
           "c.test/cross-site-with-cookie/d.test/set-cookie)"),
          ("OnCookiesAccessed(NavigationHandle, Change: "
           "c.test/cross-site-with-cookie/d.test/set-cookie)"),
          "OnCookiesAccessed(NavigationHandle, Read: d.test/set-cookie)",
          "OnCookiesAccessed(NavigationHandle, Change: d.test/set-cookie)",
          "DidFinishNavigation(d.test/set-cookie)"));
}

void AppendRedirectURL(std::vector<std::string>* urls,
                       const DIPSRedirectInfo& redirect,
                       const DIPSRedirectChainInfo& chain) {
  if (redirect.access_type != CookieAccessType::kNone) {
    urls->push_back(
        base::StrCat({FormatURL(redirect.url), " (",
                      CookieAccessTypeToString(redirect.access_type), ")"}));
  }
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulServerRedirect_URL) {
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
  bounce_detector()->SetRedirectHandlerForTesting(
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
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
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
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirectURL, &stateful_redirects));

  ASSERT_TRUE(content::NavigateToURL(web_contents, root_url));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, iframe_id, redirect_url));

  // b.test had a stateful redirect, but because it was in an iframe, we ignored
  // it.
  EXPECT_THAT(stateful_redirects, testing::IsEmpty());
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const DIPSRedirectInfo& redirect,
                    const DIPSRedirectChainInfo& chain) {
  if (redirect.access_type != CookieAccessType::kNone) {
    redirects->push_back(base::StrCat({FormatURL(chain.initial_url), " -> ",
                                       FormatURL(redirect.url), " -> ",
                                       FormatURL(chain.final_url)}));
  }
}

// Tests that a stateful client-side redirect that occurs in less than
// 10 seconds is recognized.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Client) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* frame;

  std::vector<std::string> redirects;
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  frame = web_contents->GetPrimaryMainFrame();
  // Wait for navigation to finish to initial page
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  // Advance TimeTicks 10 seconds
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(10)));
  // Navigate to interstitial page via "mouse click"
  UserActivationObserver observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce_url),
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  observer.Wait();

  // Wait for navigation to finish to interstitial page
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Advance TimeTicks by 1 second
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page
  CookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  // Initiate client-side redirect via JS without click
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", final_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to final page
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_THAT(logger->log(), testing::ElementsAre(
                                 ("DidStartNavigation(a.test/title1.html)"),
                                 ("DidFinishNavigation(a.test/title1.html)"),
                                 ("DidStartNavigation(b.test/title1.html)"),
                                 ("DidFinishNavigation(b.test/title1.html)"),
                                 ("OnCookiesAccessed(RenderFrameHost, "
                                  "Change: b.test/title1.html)"),
                                 ("DidStartNavigation(c.test/title1.html)"),
                                 ("DidFinishNavigation(c.test/title1.html)")));

  EXPECT_THAT(
      redirects,
      testing::ElementsAre(
          ("a.test/title1.html -> b.test/title1.html -> c.test/title1.html")));
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
  bounce_detector()->SetRedirectHandlerForTesting(
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

// Tests behavior for recognizing stateful client-side redirect that happens
// between stateful server-side redirects.
// TODO(https://crbug.com/1345215): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DetectStatefulRedirect_ServerClient \
  DISABLED_DetectStatefulRedirect_ServerClient
#else
#define MAYBE_DetectStatefulRedirect_ServerClient \
  DetectStatefulRedirect_ServerClient
#endif
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       MAYBE_DetectStatefulRedirect_ServerClient) {
  GURL initial1_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect1_url = embedded_test_server()->GetURL(
      "a.test", "/cross-site-with-cookie/b.test/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL initial2_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL redirect2_url = embedded_test_server()->GetURL(
      "c.test", "/cross-site-with-cookie/d.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* frame;

  std::vector<std::string> redirects;
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit initial page 1.
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial1_url));
  // Visit the redirect (a.test -> b.test with cookies).
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect1_url, bounce_url));

  // Wait for navigation to finish to bounce page (b.test).
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Advance TimeTicks by 1 second.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page.
  CookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  // Initiate client-side redirect via JS without click (to initial page 2).
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", initial2_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to redirect page 2 (c.test).
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  // Visit the redirect (c.test -> d.test with cookies).
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect2_url, final_url));

  EXPECT_THAT(logger->log(), testing::ElementsAre(
                                 ("DidStartNavigation(a.test/title1.html)"),
                                 ("DidFinishNavigation(a.test/title1.html)"),
                                 ("DidStartNavigation(a.test/"
                                  "cross-site-with-cookie/b.test/title1.html)"),
                                 ("OnCookiesAccessed(NavigationHandle, "
                                  "Change: a.test/cross-site-with-cookie/"
                                  "b.test/title1.html)"),
                                 ("DidFinishNavigation(b.test/title1.html)"),
                                 ("OnCookiesAccessed(RenderFrameHost, "
                                  "Change: b.test/title1.html)"),
                                 ("DidStartNavigation(c.test/title1.html)"),
                                 ("DidFinishNavigation(c.test/title1.html)"),
                                 ("DidStartNavigation(c.test/"
                                  "cross-site-with-cookie/d.test/title1.html)"),
                                 ("OnCookiesAccessed(NavigationHandle, "
                                  "Change: c.test/cross-site-with-cookie/"
                                  "d.test/title1.html)"),
                                 ("DidFinishNavigation(d.test/title1.html)")));

  EXPECT_THAT(
      redirects,
      testing::ElementsAre(
          ("a.test/title1.html -> "
           "a.test/cross-site-with-cookie/b.test/title1.html -> "
           "b.test/title1.html"),
          ("a.test/title1.html -> b.test/title1.html -> c.test/title1.html"),
          ("c.test/title1.html -> "
           "c.test/cross-site-with-cookie/d.test/title1.html -> "
           "d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulClientRedirect_Chain) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce1_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL bounce2_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL bounce3_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* frame;

  std::vector<std::string> redirects;
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  // Visit initial page.
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Wait for navigation to finish to initial page.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  // Advance TimeTicks 10 seconds.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(10)));
  // Navigate to bounce page 1 via "mouse click".
  UserActivationObserver observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce1_url),
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  observer.Wait();

  // Wait for navigation to finish to bounce page 1.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  // simulate mouse click
  UserActivationObserver observer2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer2.Wait();
  // Advance TimeTicks by 1 second.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page 1.
  CookieAccessObserver cookie_observer1(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer1.Wait();
  // Initiate client-side redirect via JS without click.
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce2_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to bounce page 2.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Advance TimeTicks by 1 second.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page 2.
  CookieAccessObserver cookie_observer2(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer2.Wait();
  // Initiate client-side redirect to via JS without click.
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce3_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to bounce page 3.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Advance TimeTicks by 1 second.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page 3.
  CookieAccessObserver cookie_observer3(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer3.Wait();
  // Initiate client-side redirect to final page via JS without click.
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", final_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to final page (a.test).
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // c.test and d.test are stateful bounces, but b.test is not counted as a
  // bounce because it received user activation shortly before redirecting away.
  EXPECT_THAT(
      redirects,
      testing::ElementsAre(
          ("b.test/title1.html -> c.test/title1.html -> d.test/title1.html"),
          ("c.test/title1.html -> d.test/title1.html -> a.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulServerRedirect_NoContent) {
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
  bounce_detector()->SetRedirectHandlerForTesting(
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
                       DetectStatefulServerRedirect_404Error) {
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
  bounce_detector()->SetRedirectHandlerForTesting(
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

const std::vector<std::string>& GetAllRedirectMetrics() {
  static const std::vector<std::string> kAllRedirectMetrics = {
      "ClientBounceDelay",
      "CookieAccessType",
      "HasStickyActivation",
      "InitialAndFinalSitesSame",
      "RedirectAndFinalSiteSame",
      "RedirectAndInitialSiteSame",
      "RedirectChainIndex",
      "RedirectChainLength",
      "RedirectType",
      "SiteEngagementLevel",
  };
  return kAllRedirectMetrics;
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Histograms_BounceCategory_Client) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce1_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL bounce2_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* frame;

  // Set cookies on b.test. Note that browser-initiated navigations like
  // these are treated as a sign of user engagement.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("b.test", "/set-cookie?name=value")));

  // Set cookies on c.test without of user engagement signal.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("c.test", "/set-cookie?name=value")));

  // Visit initial page.
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Wait for navigation to finish to initial page.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  // Advance TimeTicks 10 seconds.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(10)));
  // Navigate to bounce page 1 via "mouse click" and monitor the histograms.
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  UserActivationObserver observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce1_url),
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  observer.Wait();

  // Wait for navigation to finish to bounce page 1.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Wait until we can click.
  content::WaitForHitTestData(frame);
  // Advance TimeTicks by 1 second
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Initiate client-side redirect via JS without click.
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", bounce2_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to bounce page 2.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  frame = web_contents->GetPrimaryMainFrame();
  // Advance TimeTicks by 1 second.
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Write Cookie via JS on bounce page 2 (c.test).
  CookieAccessObserver cookie_observer(web_contents, frame);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  // Initiate client-side redirect to final page via JS without click.
  ASSERT_TRUE(content::ExecJs(
      frame, content::JsReplace("window.location.href = $1;", final_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for navigation to finish to final page (d.test).
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Verify the correct histogram was used for all samples.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategoryClient.Standard"] = 2;
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategoryClient."),
      testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. b.test is a has user engagement
  // and read cookies, while c.test has no user engagement and wrote cookies.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryClient.Standard"),
      testing::ElementsAre(
          // c.test
          Bucket((int)RedirectCategory::kReadWriteCookies_NoEngagement, 1),
          // b.test
          Bucket((int)RedirectCategory::kReadCookies_HasEngagement, 1)));

  // Verify a redirect time metric was recorded for each bounce.
  histograms.ExpectBucketCount(
      "Privacy.DIPS.TimeFromNavigationCommitToClientBounce",
      static_cast<base::HistogramBase::Sample>(
          base::Seconds(1).InMilliseconds()),
      /*expected_count=*/2);

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries("DIPS.Redirect", GetAllRedirectMetrics());
  ASSERT_EQ(2u, ukm_entries.size());

  EXPECT_THAT(
      FormatURL(
          ukm_recorder.GetSourceForSourceId(ukm_entries[0].source_id)->url()),
      Eq("b.test/title1.html"));
  EXPECT_THAT(ukm::GetSourceIdType(ukm_entries[0].source_id),
              Eq(ukm::SourceIdType::NAVIGATION_ID));
  EXPECT_THAT(
      ukm_entries[0].metrics,
      ElementsAre(Pair("ClientBounceDelay", 1),
                  Pair("CookieAccessType", (int)CookieAccessType::kRead),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 1),
                  Pair("RedirectType", (int)DIPSRedirectType::kClient),
                  Pair("SiteEngagementLevel", Gt((int)EngagementLevel::NONE))));

  EXPECT_THAT(
      FormatURL(
          ukm_recorder.GetSourceForSourceId(ukm_entries[1].source_id)->url()),
      Eq("c.test/title1.html"));
  EXPECT_THAT(ukm::GetSourceIdType(ukm_entries[1].source_id),
              Eq(ukm::SourceIdType::NAVIGATION_ID));
  EXPECT_THAT(
      ukm_entries[1].metrics,
      ElementsAre(Pair("ClientBounceDelay", 1),
                  Pair("CookieAccessType", (int)CookieAccessType::kReadWrite),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 1),
                  Pair("RedirectType", (int)DIPSRedirectType::kClient),
                  Pair("SiteEngagementLevel", (int)EngagementLevel::NONE)));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Histograms_BounceCategory_Server) {
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
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, redirect_url,
                                                 final_url));

  // Verify the correct histogram was used for all samples.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategoryServer.Standard"] = 2;
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategoryServer."),
      testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. Note that the a.test redirect was
  // not reported because the previously committed page was also on a.test.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryServer.Standard"),
      testing::ElementsAre(
          // c.test
          Bucket((int)RedirectCategory::kNoCookies_NoEngagement, 1),
          // b.test
          Bucket((int)RedirectCategory::kReadCookies_HasEngagement, 1)));

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries("DIPS.Redirect", GetAllRedirectMetrics());
  ASSERT_EQ(3u, ukm_entries.size());

  EXPECT_THAT(
      FormatURL(
          ukm_recorder.GetSourceForSourceId(ukm_entries[0].source_id)->url()),
      Eq("a.test/cross-site-with-cookie/b.test/cross-site/c.test/cross-site/"
         "d.test/title1.html"));
  EXPECT_THAT(ukm::GetSourceIdType(ukm_entries[0].source_id),
              Eq(ukm::SourceIdType::REDIRECT_ID));
  EXPECT_THAT(
      ukm_entries[0].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)CookieAccessType::kReadWrite),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", true),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 3),
                  Pair("RedirectType", (int)DIPSRedirectType::kServer),
                  Pair("SiteEngagementLevel", Gt((int)EngagementLevel::NONE))));

  EXPECT_THAT(
      FormatURL(
          ukm_recorder.GetSourceForSourceId(ukm_entries[1].source_id)->url()),
      Eq("b.test/cross-site/c.test/cross-site/d.test/title1.html"));
  EXPECT_THAT(ukm::GetSourceIdType(ukm_entries[1].source_id),
              Eq(ukm::SourceIdType::REDIRECT_ID));
  EXPECT_THAT(
      ukm_entries[1].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)CookieAccessType::kRead),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 1), Pair("RedirectChainLength", 3),
                  Pair("RedirectType", (int)DIPSRedirectType::kServer),
                  Pair("SiteEngagementLevel", Gt((int)EngagementLevel::NONE))));

  EXPECT_THAT(
      FormatURL(
          ukm_recorder.GetSourceForSourceId(ukm_entries[2].source_id)->url()),
      Eq("c.test/cross-site/d.test/title1.html"));
  EXPECT_THAT(ukm::GetSourceIdType(ukm_entries[2].source_id),
              Eq(ukm::SourceIdType::REDIRECT_ID));
  EXPECT_THAT(
      ukm_entries[2].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)CookieAccessType::kNone),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 2), Pair("RedirectChainLength", 3),
                  Pair("RedirectType", (int)DIPSRedirectType::kServer),
                  Pair("SiteEngagementLevel", (int)EngagementLevel::NONE)));
}

// This test verifies that a third-party cookie access doesn't cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    DetectStatefulRedirect_Client_IgnoreThirdPartySubresource) {
  // We host the image on an HTTPS server, because for it to read a third-party
  // cookie, it needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url = https_server.GetURL("d.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set SameSite=None cookie on d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_server.GetURL(
                        "d.test", "/set-cookie?foo=bar;Secure;SameSite=None")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Advance TimeTicks by 1 second
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Cause a third-party cookie read.
  CreateImageAndWaitForCookieAccess(image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              testing::ElementsAre(
                  // Set cookie on d.test
                  ("DidStartNavigation(d.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: d.test/set-cookie)"),
                  ("DidFinishNavigation(d.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading third-party d.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: d.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));

  // b.test is not considered a stateful bounce.
  EXPECT_THAT(redirects, testing::IsEmpty());
}

// This test verifies that a same-site cookie access DOES cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Client_FirstPartySubresource) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url =
      embedded_test_server()->GetURL("sub.b.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  bounce_detector()->SetRedirectHandlerForTesting(
      base::BindRepeating(&AppendRedirect, &redirects));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set cookie on sub.b.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("sub.b.test", "/set-cookie?foo=bar")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Advance TimeTicks by 1 second
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  // Cause a same-site cookie read.
  CreateImageAndWaitForCookieAccess(image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              testing::ElementsAre(
                  // Set cookie on sub.b.test
                  ("DidStartNavigation(sub.b.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: sub.b.test/set-cookie)"),
                  ("DidFinishNavigation(sub.b.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading same-site sub.b.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: sub.b.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));

  // b.test IS considered a stateful bounce, even though the cookie was read by
  // an image hosted on sub.b.test.
  EXPECT_THAT(
      redirects,
      testing::ElementsAre(
          ("a.test/title1.html -> b.test/title1.html -> c.test/title1.html")));
}
