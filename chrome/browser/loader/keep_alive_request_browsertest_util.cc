// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/keep_alive_request_browsertest_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "url/url_util.h"

std::string EncodeRelativeURL(std::string_view relative_url) {
  CHECK(relative_url.starts_with("/")) << relative_url;
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(relative_url, &buffer);
  return std::string(buffer.view());
}

ChromeKeepAliveRequestBrowserTestBase::ChromeKeepAliveRequestBrowserTestBase()
    : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS)) {}

ChromeKeepAliveRequestBrowserTestBase::
    ~ChromeKeepAliveRequestBrowserTestBase() = default;

void ChromeKeepAliveRequestBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");

  // Initialize an HTTPS server, as some in-browser throttles like
  // `variations::VariationsURLLoaderThrottle` only works in secure context.
  https_test_server_->AddDefaultHandlers(GetChromeTestDataDir());

  loaders_observer_ =
      std::make_unique<content::KeepAliveURLLoadersTestObserver>(
          web_contents()->GetBrowserContext());
}

void ChromeKeepAliveRequestBrowserTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(server()->ShutdownAndWaitUntilComplete());
}

void ChromeKeepAliveRequestBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Ignore all HTTPS certificate errors, as
  // `variations::VariationsURLLoaderThrottle` only works with google
  // domains, which are not covered by
  // `net::EmbeddedTestServer::CERT_TEST_NAMES`.
  command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
}

void ChromeKeepAliveRequestBrowserTestBase::InitFeatureList(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params) {
  feature_list_.InitWithFeaturesAndParameters(
      content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
          additional_features_and_params),
      content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
}

void ChromeKeepAliveRequestBrowserTestBase::
    LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
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
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the 1st page has been unloaded.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
  // The disconnected loader is still pending to receive response.

  // Sends back response to terminate in-browser request handling.
  keepalive_request_handler->Send(response);
  keepalive_request_handler->Done();
}

std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
ChromeKeepAliveRequestBrowserTestBase::RegisterRequestHandlers(
    const std::vector<std::string>& relative_urls) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      handlers;
  for (const auto& relative_url : relative_urls) {
    handlers.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            server(), relative_url));
  }
  return handlers;
}

GURL ChromeKeepAliveRequestBrowserTestBase::GetKeepAlivePageURL(
    const std::string& host,
    const std::string& target_relative_url,
    const std::string& method) {
  std::string url = base::StringPrintf(
      "/fetch-keepalive.html?target_url=%s&method=%s",
      EncodeRelativeURL(target_relative_url).c_str(), method.c_str());
  return server()->GetURL(host, url);
}

GURL ChromeKeepAliveRequestBrowserTestBase::GetCrossOriginPageURL() {
  return server()->GetURL(kSecondaryHost, "/title2.html");
}

void ChromeKeepAliveRequestBrowserTestBase::DisableBackForwardCache(
    content::WebContents* web_contents) {
  content::DisableBackForwardCacheForTesting(
      web_contents, content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
}

void ChromeKeepAliveRequestBrowserTestBase::SetUseHttps() {
  use_https_ = true;
}

content::WebContents* ChromeKeepAliveRequestBrowserTestBase::web_contents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderFrameHost*
ChromeKeepAliveRequestBrowserTestBase::current_frame_host() {
  return web_contents()->GetPrimaryMainFrame();
}

net::EmbeddedTestServer* ChromeKeepAliveRequestBrowserTestBase::server() {
  return use_https_ ? https_test_server_.get() : embedded_test_server();
}

content::KeepAliveURLLoadersTestObserver&
ChromeKeepAliveRequestBrowserTestBase::loaders_observer() {
  return *loaders_observer_;
}
