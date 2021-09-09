// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kOriginTrialOrigin[] = "https://a.test";

class CryptotokenBrowserTest : public base::test::WithFeatureOverride,
                               public InProcessBrowserTest {
 protected:
  CryptotokenBrowserTest()
      : base::test::WithFeatureOverride(
            extensions_features::kU2FSecurityKeyAPI) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default privatey key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void SetUp() override {
    // Make sure the Hangout Services component extension gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &CryptotokenBrowserTest::InterceptRequest, base::Unretained(this)));

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void ExpectConnectResult(bool expected_result) {
    const std::string script = R"((() => {
          let port = chrome.runtime.connect("kmendfapggjehodndflmmgagdbamhnfd",
              {});
          return !!port;
      })())";
    const content::EvalJsResult result = content::EvalJs(
        browser()->tab_strip_model()->GetActiveWebContents(), script);
    if (expected_result) {
      EXPECT_TRUE(result.ExtractBool());
    } else {
      // chrome.runtime will be undefined because there is no connectable
      // extension for this origin.
      EXPECT_THAT(
          result.error,
          testing::StartsWith("a JavaScript error:\nTypeError: Cannot read "
                              "properties of undefined (reading 'connect')"));
    }
  }

  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  // content::URLLoaderInterceptor callback
  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    // The response for origin trial requests are injected so that we have a
    // stable port to use for generating the token.
    if (params->url_request.url.GetOrigin() != GURL(kOriginTrialOrigin)) {
      return false;
    }

    // Generated with `tools/origin_trials/generate_token.py --expire-days 5000
    // https://a.test U2FSecurityKeyAPI`
    constexpr char kOriginTrialToken[] =
        "A5xaE9lySzMBejK4rKNqBC86X9m2VhMwB/"
        "1FXWcHNhtPhdTK02TzChhjmD7p+"
        "kMn2tTO424RwoUWlFCLsd1tRAQAAABWeyJvcmlnaW4iOiAiaHR0cHM6Ly9hLnRlc3Q6NDQ"
        "zIiwgImZlYXR1cmUiOiAiVTJGU2VjdXJpdHlLZXlBUEkiLCAiZXhwaXJ5IjogMjA2MjcxM"
        "DYzNH0=";

    const std::string headers = base::StringPrintf(
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n"
        "Origin-Trial: %s\n\n",
        kOriginTrialToken);
    content::URLLoaderInterceptor::WriteResponse(
        headers, "<html><head></head><body>OK</body></html>",
        params->client.get());
    return true;
  }

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, Connect) {
  // CryptoToken can only be connected to if the feature flag is enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("a.test", "/empty.html")));
  ExpectConnectResult(IsParamFeatureEnabled());
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, ConnectWithOriginTrial) {
  // Connection succeeds regardless of feature flag state with the origin trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(kOriginTrialOrigin)));
  ExpectConnectResult(true);
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, InsecureOriginCannotConnect) {
  // Connections from insecure origins always fail.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_server_.GetURL("a.test", "/empty.html")));
  ExpectConnectResult(false);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(CryptotokenBrowserTest);

}  // namespace
}  // namespace extensions
