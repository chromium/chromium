// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Web Workers (a Content feature) work in the Chromium
// embedder.

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

enum class EnableThirdPartyCookieBlocking { kEnable, kDisable };

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
      EnableThirdPartyCookieBlocking enable_third_party_cookie_blocking,
      const std::string& test_url) {
    const std::string kCookie = "foo=bar";

    // Set up third-party cookie blocking.
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kBlockThirdPartyCookies,
        enable_third_party_cookie_blocking ==
            EnableThirdPartyCookieBlocking::kEnable);

    // Make sure cookies are not set.
    ASSERT_TRUE(
        GetCookies(browser()->profile(), embedded_test_server()->base_url())
            .empty());

    // Request for the worker script should not send cookies.
    {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      ui_test_utils::NavigateToURL(browser(),
                                   embedded_test_server()->GetURL(test_url));
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
      ui_test_utils::NavigateToURL(browser(),
                                   embedded_test_server()->GetURL(test_url));
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
      EnableThirdPartyCookieBlocking::kEnable,
      "/workers/create_dedicated_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       DedicatedWorkerScriptFetchWithoutThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      EnableThirdPartyCookieBlocking::kDisable,
      "/workers/create_dedicated_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       SharedWorkerScriptFetchWithThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      EnableThirdPartyCookieBlocking::kEnable,
      "/workers/create_shared_worker.html?worker_url=/capture");
}

IN_PROC_BROWSER_TEST_F(ChromeWorkerBrowserTest,
                       SharedWorkerScriptFetchWithoutThirdPartyBlocking) {
  TestWorkerScriptFetchWithThirdPartyCookieBlocking(
      EnableThirdPartyCookieBlocking::kDisable,
      "/workers/create_shared_worker.html?worker_url=/capture");
}
