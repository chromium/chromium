// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/platform_browser_test.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class RenderFrameHost;
class KeepAliveURLLoadersTestObserver;
}  // namespace content

inline constexpr char kPrimaryHost[] = "a.com";
inline constexpr char kSecondaryHost[] = "b.com";
inline constexpr char kGoogleHost[] = "www.google.com";
inline constexpr char kKeepAliveEndpoint[] = "/beacon";
inline constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

inline constexpr char k200TextResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";
inline constexpr char k301ResponseTemplate[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: %s\r\n"
    "\r\n";

// Encodes a relative URL to a string that can be used in a URL.
// `relative_url` must start with "/".
std::string EncodeRelativeURL(std::string_view relative_url);

// Base class for browser tests that need to test fetch keepalive requests.
//
// This class provides common utilities to set up the test environment and
// navigate to pages with fetch keepalive requests.
class ChromeKeepAliveRequestBrowserTestBase : public PlatformBrowserTest {
 public:
  ChromeKeepAliveRequestBrowserTestBase();
  ~ChromeKeepAliveRequestBrowserTestBase() override;
  // Not copyable.
  ChromeKeepAliveRequestBrowserTestBase(
      const ChromeKeepAliveRequestBrowserTestBase&) = delete;
  ChromeKeepAliveRequestBrowserTestBase& operator=(
      const ChromeKeepAliveRequestBrowserTestBase&) = delete;

 protected:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Sets the test server to use HTTPS.
  void SetUseHttps();

  // Initializes the feature list with BackForwardCache features and
  // `additional_features_and_params`.
  void InitFeatureList(const std::vector<base::test::FeatureRefAndParams>&
                           additional_features_and_params);

  // Navigates to a page specified by `keepalive_page_url`, which must fires a
  // fetch keepalive request.
  // The method then postpones the request handling until RFH of the page is
  // fully unloaded (by navigating to another cross-origin page).
  // After that, `response` will be sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response);

  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls);

  GURL GetKeepAlivePageURL(const std::string& host,
                           const std::string& target_relative_url,
                           const std::string& method);

  GURL GetCrossOriginPageURL();

  void DisableBackForwardCache(content::WebContents* web_contents);

  content::WebContents* web_contents();

  content::RenderFrameHost* current_frame_host();

  net::EmbeddedTestServer* server();

  content::KeepAliveURLLoadersTestObserver& loaders_observer();

 private:
  base::test::ScopedFeatureList feature_list_;
  bool use_https_ = false;
  const std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<content::KeepAliveURLLoadersTestObserver> loaders_observer_;
};

#endif  // CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_
