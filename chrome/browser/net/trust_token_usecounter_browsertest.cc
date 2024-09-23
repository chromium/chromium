// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

// The Trust Token API (https://github.com/wicg/trust-token-api) allows clients
// to annotate outgoing requests with certain cryptographic state, by providing
// parameters alongside Fetch, XHR, and iframe-tag loads. These tests confirm
// that providing Trust Tokens parameters to each of these API entry points
// increments the corresponding Blink UseCounter. Although Trust Tokens is part
// of the web platform and the use counters are incremented in Blink, these
// tests live in //chrome because the Page Load Metrics framework, Chrome's way
// of listening for use counter updates, is not available in the content shell.

class TrustTokenUseCountersBrowsertest : public InProcessBrowserTest {
 public:
  TrustTokenUseCountersBrowsertest() {
    features_.InitAndEnableFeature(network::features::kPrivateStateTokens);
  }

  void SetUpOnMainThread() override {
    server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(server_.Start());
  }

 protected:
  base::test::ScopedFeatureList features_;

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(TrustTokenUseCountersBrowsertest, CountsFetchUse) {
  base::HistogramTester histograms;

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/page404.html", {privateToken: {version: 1,
                                               operation: 'token-request'}});
  } )(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve. We expect a failure because the destination of the Trust
  // Tokens operation-bearing request will 404; this is fine since we just need
  // to test that the usage metric fired when the command was called.
  EXPECT_FALSE(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), cmd)
          .error.empty());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histograms.ExpectBucketCount("Blink.UseCounter.Features",
                               blink::mojom::WebFeature::kTrustTokenFetch, 1);
}

IN_PROC_BROWSER_TEST_F(TrustTokenUseCountersBrowsertest, CountsXhrUse) {
  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  base::HistogramTester histograms;

  // Execute a Trust Tokens issuance against a nonexistent endpoint.
  std::string cmd = R"(
  (async () => {
    let request = new XMLHttpRequest();
    request.open('GET', '/page404.html');
    request.setPrivateToken({
      version: 1,
      operation: 'token-request'
    });
    let promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;
    })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve. We expect a failure because the destination of the Trust
  // Tokens operation-bearing request will 404; this is fine since we just need
  // to test that the usage metric fired when the command was called.
  EXPECT_FALSE(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), cmd)
          .error.empty());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histograms.ExpectBucketCount("Blink.UseCounter.Features",
                               blink::mojom::WebFeature::kTrustTokenXhr, 1);
}

IN_PROC_BROWSER_TEST_F(TrustTokenUseCountersBrowsertest, CountsIframeUse) {
  base::HistogramTester histograms;

  GURL start_url(server_.GetURL("/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // It's important to set the trust token arguments before updating src, as
  // the latter triggers a load. It's also important to JsReplace the trustToken
  // argument here, because iframe.privateToken expects a (properly escaped)
  // JSON-encoded string as its value, not a JS object.
  EXPECT_TRUE(ExecJs(web_contents,
                     JsReplace(
                         R"( const myFrame = document.getElementById("test");
                         myFrame.privateToken = $1;
                         myFrame.src = $2;)",
                         R"({"version": 1,
                            "operation": "send-redemption-record"})",
                         "/page404.html")));
  TestNavigationObserver load_observer(web_contents);
  load_observer.Wait();

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histograms.ExpectBucketCount("Blink.UseCounter.Features",
                               blink::mojom::WebFeature::kTrustTokenIframe, 1);
}

IN_PROC_BROWSER_TEST_F(TrustTokenUseCountersBrowsertest, CountsIframeUseViaSetattribute) {
  base::HistogramTester histograms;

  GURL start_url(server_.GetURL("/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // It's important to set the trust token arguments before updating src, as
  // the latter triggers a load. It's also important to JsReplace the trustToken
  // argument here, because iframe.trustToken expects a (properly escaped)
  // JSON-encoded string as its value, not a JS object.
  EXPECT_TRUE(ExecJs(web_contents,
                     JsReplace(
                         R"( const myFrame = document.getElementById("test");
                         myFrame.setAttribute('privateToken', $1);
                         myFrame.src = $2;)",
                         R"({"version": 1,
                            "operation": "send-redemption-record"})",
                         "/page404.html")));
  TestNavigationObserver load_observer(web_contents);
  load_observer.Wait();

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histograms.ExpectBucketCount("Blink.UseCounter.Features",
                               blink::mojom::WebFeature::kTrustTokenIframe, 1);
}

}  // namespace content
