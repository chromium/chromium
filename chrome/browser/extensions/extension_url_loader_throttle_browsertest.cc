// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "net/base/backoff_entry.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace extensions {

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    bool set_cache_header_redirect_page,
    bool set_cache_header_test_throttle_page,
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, "/redirect",
                       base::CompareCase::SENSITIVE)) {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_FOUND);
    http_response->set_content("Redirecting...");
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader("Location", "/test_throttle");
    if (set_cache_header_redirect_page)
      http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return std::move(http_response);
  }

  if (base::StartsWith(request.relative_url, "/test_throttle",
                       base::CompareCase::SENSITIVE)) {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
    http_response->set_content("The server is overloaded right now.");
    http_response->set_content_type("text/plain");
    if (set_cache_header_test_throttle_page)
      http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return std::move(http_response);
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return std::unique_ptr<net::test_server::BasicHttpResponse>();
}

}  // namespace

class ExtensionURLLoaderThrottleBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kSetExtensionThrottleTestParams);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    // Requests to 127.0.0.1 bypass throttling, so set up a host resolver rule
    // to use a fake domain.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    extension_ =
        LoadExtension(test_data_dir_.AppendASCII("extension_throttle"));
    ASSERT_TRUE(extension_);
  }

  void RunTest(const std::string& file_path,
               const std::string& request_url,
               const std::string& expected_throttled_request_num) {
    ResultCatcher catcher;
    const GURL unthrottled_test_url = net::AppendQueryParameter(
        extension_->GetResourceURL(file_path), "url", request_url);
    const GURL test_url =
        expected_throttled_request_num.empty()
            ? unthrottled_test_url
            : net::AppendQueryParameter(unthrottled_test_url,
                                        "expectedFailRequestNum",
                                        expected_throttled_request_num);
    ui_test_utils::NavigateToURL(browser(), test_url);
    ASSERT_TRUE(catcher.GetNextResult());
  }

 private:
  const Extension* extension_;
};

class ExtensionURLLoaderThrottleWithSplitCacheBrowserTest
    : public ExtensionURLLoaderThrottleBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    bool split_cache_by_network_isolation_key = GetParam();
    feature_list_.InitWithFeatureState(
        net::features::kSplitCacheByNetworkIsolationKey,
        split_cache_by_network_isolation_key);
    ExtensionURLLoaderThrottleBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that if the same URL is requested repeatedly by an extension, it will
// eventually be throttled.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       ThrottleRequest) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, false, false));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              "3"));
}

// Tests that if the same URL is repeatedly requested by an extension, and the
// response is served from the cache, it will not be throttled.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       DoNotThrottleCachedResponse) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, false, true));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              ""));
}

// Tests that the redirected request is also being throttled.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       ThrottleRequest_Redirect) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, false, false));
  ASSERT_TRUE(embedded_test_server()->Start());
  // Issue a bunch of requests to a url which gets redirected to a new url that
  // generates 503.
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/redirect",
                                 embedded_test_server()->port()),
              "3"));

  // Now requests to both URLs should be throttled. Explicitly validate that the
  // second URL is throttled.
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              "1"));
}

// Tests that if both redirect (302) and non-redirect (503) responses are
// served from cache, the extension throttle does not throttle the request.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       DoNotThrottleCachedResponse_Redirect) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, true, true));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/redirect",
                                 embedded_test_server()->port()),
              ""));
}

// Tests that if the redirect (302) is served from cache, but the non-redirect
// (503) is not, the extension throttle throttles the requests for the second
// url.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       ThrottleRequest_RedirectCached) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, true, false));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/redirect",
                                 embedded_test_server()->port()),
              "3"));

  // Explicitly validate that the second URL is throttled.
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              "1"));
}

// Tests that if the redirect (302) is not served from cache, but the
// non-redirect (503) is, the extension throttle only throttles requests to the
// redirect URL.
IN_PROC_BROWSER_TEST_P(ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
                       DoNotThrottleCachedResponse_NonRedirectCached) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, false, true));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/redirect",
                                 embedded_test_server()->port()),
              ""));

  // Explicitly validate that the second URL is not throttled.
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              ""));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionURLLoaderThrottleWithSplitCacheBrowserTest,
    testing::Bool());

class ExtensionURLLoaderThrottleCommandLineBrowserTest
    : public ExtensionURLLoaderThrottleBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionURLLoaderThrottleBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kDisableExtensionsHttpThrottling);
  }
};

// Tests that if switches::kDisableExtensionsHttpThrottling is set on the
// command line, throttling is disabled.
IN_PROC_BROWSER_TEST_F(ExtensionURLLoaderThrottleCommandLineBrowserTest,
                       ThrottleRequestDisabled) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, false, false));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_request_throttle.html",
              base::StringPrintf("http://www.example.com:%d/test_throttle",
                                 embedded_test_server()->port()),
              ""));
}

}  // namespace extensions
