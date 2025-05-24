// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/loader/keep_alive_request_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_util.h"

namespace {

using testing::Contains;
using testing::Key;
using testing::Not;

}  // namespace

class ChromeKeepAliveURLBrowserTestBase
    : public ChromeKeepAliveRequestBrowserTestBase {
 public:
  ChromeKeepAliveURLBrowserTestBase() {
    InitFeatureList({{blink::features::kKeepAliveInBrowserMigration, {}}});
  }
  ~ChromeKeepAliveURLBrowserTestBase() override = default;

  // Not copyable.
  ChromeKeepAliveURLBrowserTestBase(const ChromeKeepAliveURLBrowserTestBase&) =
      delete;
  ChromeKeepAliveURLBrowserTestBase& operator=(
      const ChromeKeepAliveURLBrowserTestBase&) = delete;
};

// Basic Chrome browser tests to cover behaviors when handling fetch keepalive
// requests in browser process.
//
// Tests here ensure the behaviors are the same as their counterparts in
// `content/browser` even with extra logic added by Chrome embedder.
class ChromeKeepAliveURLBrowserTest
    : public ChromeKeepAliveURLBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  const std::string& method() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeKeepAliveURLBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<ChromeKeepAliveURLBrowserTest::ParamType>&
           info) { return info.param; });

IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest, OneRequest) {
  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(
      web_contents(), GetKeepAlivePageURL(kPrimaryHost, target_url, method())));
  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();

  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  content::TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping has been unloaded. The browser must ensure the response is received and
// processed by the browser.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, target_url, method()),
      request_handler.get(), k200TextResponse);

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
}

// Shutdown delay is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
// Mac browser shutdown is flaky: https://crbug.com/1259913
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReceiveResponseAfterBrowserShutdown \
  DISABLED_ReceiveResponseAfterBrowserShutdown
#else
#define MAYBE_ReceiveResponseAfterBrowserShutdown \
  ReceiveResponseAfterBrowserShutdown
#endif
// Verifies that a keepalive ping can be made within a short timeframe after
// browser shutdown.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest,
                       MAYBE_ReceiveResponseAfterBrowserShutdown) {
  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());
  auto keepalive_page_url =
      GetKeepAlivePageURL(kPrimaryHost, target_url, method());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), keepalive_page_url));

  // Close the browser.
  CloseBrowserSynchronously(browser());
  ASSERT_TRUE(browser_shutdown::IsTryingToQuit());
  ASSERT_TRUE(BrowserList::GetInstance()->empty());
  ASSERT_EQ(browser_shutdown::GetShutdownType(),
            browser_shutdown::ShutdownType::kWindowClose);
  // The keepalive request may be sent before or after shutting down, but only
  // get processed by the server after shutting down here.
  request_handler->WaitForRequest();
  // The disconnected loader is pending to receive response.

  // Send back response to terminate in-browser request handling.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  // The response should be processed by browser before shutting down.
  // TODO(crbug.com/40236167): Deflake WaitForTotalOnReceiveResponseProcessed
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(
      web_contents(), GetKeepAlivePageURL(kPrimaryHost, target_url, method())));
  content::RenderFrameHostWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  request_handler->WaitForRequest();

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been put into BackForwardCache.
  ASSERT_EQ(rfh_1->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Send back response.
  request_handler->Send(k200TextResponse);
  // The response is immediately forwarded to the in-BackForwardCache renderer.
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  // Go back to `rfh_1`.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The response should be processed in renderer. Hence resolving Promise.
  content::TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
}

// Delays handling redirect for a keepalive ping until after the page making the
// keepalive ping has been unloaded. The browser must ensure the redirect is
// verified and properly processed by the browser.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string target_url = kKeepAliveEndpoint;
  const char redirect_target[] = "/beacon-redirected";
  auto request_handlers =
      RegisterRequestHandlers({target_url, redirect_target});
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://a.com:<port>/beacon-redirected
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, target_url, method()),
      request_handlers[0].get(),
      base::StringPrintf(k301ResponseTemplate, redirect_target));

  // The redirect request should be processed in browser and gets sent.
  request_handlers[1]->WaitForRequest();
  // No variations header when redirected to non-Google.
  EXPECT_THAT(request_handlers[1]->http_request()->headers,
              Not(Contains(Key(variations::kClientDataHeader))));
  // End the keepalive request by sending back final response.
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[1]->Done();

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
}

// Delays handling an unsafe redirect for a keepalive ping until after the page
// making the keepalive ping has been unloaded.
// The browser must ensure the unsafe redirect is not followed.
// TODO(crbug.com/407716208): Broken by crrev.com/c/6039011. Fix and re-enable
// this test.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLBrowserTest,
                       DISABLED_ReceiveUnSafeRedirectAfterPageUnload) {
  const std::string target_url = kKeepAliveEndpoint;
  const char unsafe_redirect_target[] = "chrome://settings";
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> chrome://settings
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, target_url, method()),
      request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         unsafe_redirect_target));

  // The redirect is unsafe, so the loader is terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed(
      {net::ERR_UNSAFE_REDIRECT});
  // The test is too flaky to assert on the number of redirects processed.
}

// Checks that when a fetch keepalive request's redirect is handled in browser
// the variations header (X-Client-Data) is attached to the requests to Google.
// TODO(crbug.com/407998594): Fix test before re-enabling.
IN_PROC_BROWSER_TEST_P(
    ChromeKeepAliveURLBrowserTest,
    DISABLED_ReceiveMultipleRedirectsToGoogleAfterPageUnload) {
  const std::string target_url = kKeepAliveEndpoint;
  const std::string redirect_target1 = "/redirected1";
  const std::string redirect_target2 = "/redirected2";
  SetUseHttps();
  auto request_handlers =
      RegisterRequestHandlers({target_url, redirect_target1, redirect_target2});
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("https://www.google.com:<port>/beacon", keepalive: true)
  // --> https://www.google.com:<port>/redirected1
  // --> https://www.google.com:<port>/redirected2
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kGoogleHost, target_url, method()),
      request_handlers[0].get(),
      base::StringPrintf(k301ResponseTemplate, redirect_target1.c_str()));

  request_handlers[1]->WaitForRequest();
  EXPECT_THAT(request_handlers[1]->http_request()->headers,
              Contains(Key(variations::kClientDataHeader)));
  request_handlers[1]->Send(
      base::StringPrintf(k301ResponseTemplate, redirect_target2.c_str()));
  request_handlers[1]->Done();

  request_handlers[2]->WaitForRequest();
  EXPECT_THAT(request_handlers[2]->http_request()->headers,
              Contains(Key(variations::kClientDataHeader)));
  // End the keepalive request by sending back final response.
  request_handlers[2]->Send(k200TextResponse);
  request_handlers[2]->Done();

  // The redirects/response should all be processed in browser.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(2);
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
}

// Chrome browser tests to cover variation header-related behaviors for fetch
// keepalive requests.
class ChromeKeepAliveURLVariationBrowserTest
    : public ChromeKeepAliveURLBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  const std::string& method() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeKeepAliveURLVariationBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<
        ChromeKeepAliveURLVariationBrowserTest::ParamType>& info) {
      return info.param;
    });

// Verifies that the variations header (X-Client-Data) is attached to network
// requests to Google, but stripped on redirects to non-Google.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveURLVariationBrowserTest,
                       ReceiveRedirectToGoogleAfterPageUnloadAndStripHeaders) {
  const std::string target_url = kKeepAliveEndpoint;
  SetUseHttps();
  auto request_handlers =
      RegisterRequestHandlers({target_url, "/redirect", "/final"});
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("https://www.google.com:<port>/beacon", keepalive: true)
  // --> https://www.google.com:<port>/redirect
  // --> https://www.b.com:<port>/final
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kGoogleHost, target_url, method()),
      request_handlers[0].get(),
      ("HTTP/1.1 301 Moved Permanently\r\n"
       "Access-Control-Allow-Origin: *\r\n"
       "Location: /redirect\r\n"
       "\r\n"));

  // The 1st redirect should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);
  // This redirect request to Google should contain variation header.
  request_handlers[1]->WaitForRequest();
  EXPECT_THAT(request_handlers[1]->http_request()->headers,
              testing::Contains(testing::Key(variations::kClientDataHeader)));

  // The 2nd redirect request should contain no variation header.
  request_handlers[1]->Send(
      "HTTP/1.1 301 Moved Permanently\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Location: https://b.com/final\r\n"
      "\r\n");
  request_handlers[1]->Done();
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(2);
  // The request is dropped by network service, so no way to verify variation
  // header from `request_handlers[2]`.
}
