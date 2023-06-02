// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "third_party/blink/public/common/features.h"

namespace {

using testing::Contains;
using testing::Key;
using testing::Not;

constexpr char kPrimaryHost[] = "a.com";
constexpr char kSecondaryHost[] = "b.com";

constexpr char kKeepAliveEndpoint[] = "/beacon";

constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

constexpr char k200TextResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";
constexpr char k301ResponseTemplate[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: %s\r\n"
    "\r\n";

}  // namespace

class KeepAliveURLBrowserTestBase : public InProcessBrowserTest {
 public:
  KeepAliveURLBrowserTestBase()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~KeepAliveURLBrowserTestBase() override = default;
  // Not copyable.
  KeepAliveURLBrowserTestBase(const KeepAliveURLBrowserTestBase&) = delete;
  KeepAliveURLBrowserTestBase& operator=(const KeepAliveURLBrowserTestBase&) =
      delete;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Initialize an HTTPS server, as `variations::VariationsURLLoaderThrottle`
    // only works in secure context.
    https_test_server_->AddDefaultHandlers(GetChromeTestDataDir());

    loaders_observer_ =
        std::make_unique<content::KeepAliveURLLoadersTestObserver>(
            web_contents()->GetBrowserContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ignore all HTTPS certificate errors, as
    // `variations::VariationsURLLoaderThrottle` only works with google domains,
    // which are not covered by `net::EmbeddedTestServer::CERT_TEST_NAMES`.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  // Navigates to a page specified by `keepalive_page_url`, which must fires a
  // fetch keepalive request.
  // The method then postpones the request handling until RFH of the page is
  // fully unloaded (by navigating to another cross-origin page).
  // After that, `response` will be sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response) {
    ASSERT_TRUE(content::NavigateToURL(web_contents(), keepalive_page_url));
    content::RenderFrameHostWrapper rfh_1(current_frame_host());
    // Ensure the current page can be unloaded instead of being cached.
    DisableBackForwardCache(web_contents());
    // Ensure the keepalive request is sent before leaving the current page.
    keepalive_request_handler->WaitForRequest();

    // Navigate to cross-origin page to ensure the 1st page can be unloaded.
    ASSERT_TRUE(
        content::NavigateToURL(web_contents(), GetCrossOriginPageURL()));
    // Ensure the 1st page has been unloaded.
    ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
    // The disconnected loader is still pending to receive response.

    // Sends back response to terminate in-browser request handling.
    keepalive_request_handler->Send(response);
    keepalive_request_handler->Done();
  }

  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls) {
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        handlers;
    for (const auto& relative_url : relative_urls) {
      handlers.emplace_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              server(), relative_url));
    }
    return handlers;
  }

  GURL GetKeepAlivePageURL(const std::string& host, const std::string& method) {
    std::string url =
        base::StringPrintf("/fetch-keepalive.html?method=%s", method.c_str());
    return server()->GetURL(host, url);
  }

  GURL GetCrossOriginPageURL() {
    return server()->GetURL(kSecondaryHost, "/title2.html");
  }

  void DisableBackForwardCache(content::WebContents* web_contents) {
    content::DisableBackForwardCacheForTesting(
        web_contents, content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  void SetUseHttps() { use_https_ = true; }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  net::EmbeddedTestServer* server() {
    return use_https_ ? https_test_server_.get() : embedded_test_server();
  }

  content::KeepAliveURLLoadersTestObserver& loaders_observer() {
    return *loaders_observer_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bool use_https_ = false;
  const std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<content::KeepAliveURLLoadersTestObserver> loaders_observer_;
};

// Basic Chrome browser tests to cover behaviors when handling fetch keepalive
// requests in browser process.
//
// Tests here ensure the behaviors are the same as their counterparts in
// `content/browser` even with extra logic added by Chrome embedder.
class KeepAliveURLBrowserTest
    : public KeepAliveURLBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveURLBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, OneRequest) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepAlivePageURL(kPrimaryHost, method)));
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
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, method), request_handler.get(),
      k200TextResponse);

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepAlivePageURL(kPrimaryHost, method)));
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
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char redirect_target[] = "/beacon-redirected";
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, redirect_target});
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://a.com:<port>/beacon-redirected
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, method), request_handlers[0].get(),
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
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveUnSafeRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char unsafe_redirect_target[] = "chrome://settings";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> chrome://settings
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, method), request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         unsafe_redirect_target));

  // The redirect is unsafe, so the loader is terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed(
      {net::ERR_UNSAFE_REDIRECT});
}
