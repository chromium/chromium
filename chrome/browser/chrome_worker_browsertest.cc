// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Web Workers (a Content feature) work in the Chromium
// embedder.

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/re2/src/re2/re2.h"

// A simple fixture used for testing dedicated workers and shared workers. The
// fixture stashes the HTTP request to the worker script for inspecting the
// headers.
//
// This is in //chrome instead of //content since the tests exercise the
// |kBlockThirdPartyCookies| preference which is not a //content concept.
class ChromeWorkerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ChromeWorkerBrowserTest::CaptureHeaderHandler,
                            base::Unretained(this), "/capture"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

 protected:
  // Tests worker script fetch (always same-origin) is not affected by the
  // third-party cookie blocking configuration.
  // This is the regression test for https://crbug.com/933287.
  void TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      content_settings::CookieControlsMode cookie_controls_mode,
      const std::string& test_url) {
    const std::string kCookie = "foo=bar";

    // Set up third-party cookie blocking.
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode, static_cast<int>(cookie_controls_mode));

    // Make sure cookies are not set.
    ASSERT_TRUE(
        GetCookies(browser()->profile(), embedded_test_server()->base_url())
            .empty());

    // Request for the worker script should not send cookies.
    {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), embedded_test_server()->GetURL(test_url)));
      run_loop.Run();
      EXPECT_FALSE(base::Contains(header_map_, "Cookie"));
    }

    // Set a cookie.
    ASSERT_TRUE(SetCookie(browser()->profile(),
                          embedded_test_server()->base_url(), kCookie));

    // Request for the worker script should send the cookie regardless of the
    // third-party cookie blocking configuration.
    {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), embedded_test_server()->GetURL(test_url)));
      run_loop.Run();
      EXPECT_TRUE(base::Contains(header_map_, "Cookie"));
      EXPECT_EQ(kCookie, header_map_["Cookie"]);
    }
  }

  // TODO(nhiroki): Add tests for creating workers from third-party iframes
  // while third-party cookie blocking is enabled. This expects that cookies are
  // not blocked.

 private:
  std::unique_ptr<net::test_server::HttpResponse> CaptureHeaderHandler(
      const std::string& path,
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != path)
      return nullptr;
    // Stash the HTTP request headers.
    header_map_ = request.headers;
    std::move(quit_closure_).Run();
    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

  net::test_server::HttpRequest::HeaderMap header_map_;
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       DedicatedWorkerScriptFetchWithThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      content_settings::CookieControlsMode::kBlockThirdParty,
      "/workers/create_dedicated_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       DedicatedWorkerScriptFetchWithoutThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      content_settings::CookieControlsMode::kOff,
      "/workers/create_dedicated_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       SharedWorkerScriptFetchWithThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      content_settings::CookieControlsMode::kBlockThirdParty,
      "/workers/create_shared_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       SharedWorkerScriptFetchWithoutThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      content_settings::CookieControlsMode::kOff,
      "/workers/create_shared_worker.html?worker_url=/capture");
}

// A test fixture used for testing that dedicated and shared workers have the
// correct user agent value, it should always be the reduced user agent string.
class ChromeWorkerUserAgentBrowserTest : public InProcessBrowserTest {
 public:
  ChromeWorkerUserAgentBrowserTest() = default;

  // The URL that was used to test user-agent.
  static constexpr char kOriginUrl[] = "https://127.0.0.1:44444";

  // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
  // EmbeddedTestServer serves content on a random port.
  std::unique_ptr<content::URLLoaderInterceptor> CreateUrlLoaderInterceptor() {
    return std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              if (expected_request_urls_.find(params->url_request.url) ==
                  expected_request_urls_.end())
                return false;

              std::string path = "chrome/test/data/workers";
              path.append(std::string(params->url_request.url.path_piece()));

              std::string headers = "HTTP/1.1 200 OK\n";
              base::StrAppend(
                  &headers,
                  {"Content-Type: text/",
                   base::EndsWith(params->url_request.url.path_piece(), ".js")
                       ? "javascript"
                       : "html",
                   "\n"});
              content::URLLoaderInterceptor::WriteResponse(
                  path, params->client.get(), &headers);

              return true;
            }));
  }

  void SetExpectedRequestURLs(const std::set<GURL>& expected_request_urls) {
    expected_request_urls_ = expected_request_urls;
  }

  // Check whether the user agent minor version matches "0.0.0" and we expect
  // the user agent always to be reduced.
  void CheckUserAgentString(const std::string& user_agent_value) {
    // A regular expression that matches Chrome/{major_version}.{minor_version}
    // in the User-Agent string, where the {minor_version} is captured.
    static constexpr char kChromeVersionRegex[] =
        "Chrome/[0-9]+\\.([0-9]+\\.[0-9]+\\.[0-9]+)";
    // The minor version in the reduced UA string is always "0.0.0".
    static constexpr char kReducedMinorVersion[] = "0.0.0";

    std::string minor_version;
    EXPECT_TRUE(re2::RE2::PartialMatch(user_agent_value, kChromeVersionRegex,
                                       &minor_version));

    EXPECT_EQ(minor_version, kReducedMinorVersion);
  }

 private:
  std::set<GURL> expected_request_urls_;
};

constexpr char ChromeWorkerUserAgentBrowserTest::kOriginUrl[];

IN_PROC_BROWSER_TEST_F(ChromeWorkerUserAgentBrowserTest, SharedWorker) {
  const GURL main_page_url = GURL(base::StrCat(
      {kOriginUrl,
       "/create_shared_worker.html?worker_url=onconnect_user_agent.js"}));
  const GURL worker_url =
      GURL(base::StrCat({kOriginUrl, "/onconnect_user_agent.js"}));
  SetExpectedRequestURLs({main_page_url, worker_url});

  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      CreateUrlLoaderInterceptor();

  // Navigate to the page that has the scripts for registering the worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_page_url));

  // Check the result of navigator.userAgent called from the worker.
  CheckUserAgentString(
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             "waitForMessage()")
          .ExtractString());
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerUserAgentBrowserTest,
                       DedicatedWorkerCreatedFromFrame) {
  const GURL main_page_url = GURL(base::StrCat(
      {kOriginUrl, "/create_dedicated_worker.html?worker_url=user_agent.js"}));
  const GURL worker_url = GURL(base::StrCat({kOriginUrl, "/user_agent.js"}));
  SetExpectedRequestURLs({main_page_url, worker_url});

  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      CreateUrlLoaderInterceptor();

  // Navigate to the page that has the scripts for registering the worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_page_url));

  // Check the result of navigator.userAgent called from the worker.
  CheckUserAgentString(
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             "waitForMessage()")
          .ExtractString());
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerUserAgentBrowserTest,
                       DedicatedWorkerCreatedFromDedicatedWorker) {
  const GURL main_page_url =
      GURL(base::StrCat({kOriginUrl,
                         "/create_dedicated_worker.html?worker_url=parent_"
                         "worker_user_agent.js"}));
  const GURL worker_url =
      GURL(base::StrCat({kOriginUrl, "/parent_worker_user_agent.js"}));
  const GURL user_agent_url =
      GURL(base::StrCat({kOriginUrl, "/user_agent.js"}));
  SetExpectedRequestURLs({main_page_url, worker_url, user_agent_url});

  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      CreateUrlLoaderInterceptor();

  // Navigate to the page that has the scripts for registering the worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_page_url));

  // Check the result of navigator.userAgent called from the worker.
  CheckUserAgentString(
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             "waitForMessage()")
          .ExtractString());
}
