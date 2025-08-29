// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class IframeCspTest : public PaymentRequestPlatformBrowserTestBase {
 public:
  IframeCspTest() = default;
  ~IframeCspTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    // kylepay.test is a payment app that supports just-in-time installation.
    app_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.test");
    ASSERT_TRUE(app_server_.Start());
  }

 protected:
  net::EmbeddedTestServer app_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(IframeCspTest, Show) {
  NavigateTo("/csp_test_main.html");

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  // Filter for console messages related to the CSP failure. There should be
  // none.
  console_observer.SetPattern(
      "Refused to load the image 'https://kylepay.test:*/icon.png *");

  GURL iframe_url =
      https_server()->GetURL("other.example", "/csp_test_iframe.html");
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      GetActiveWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  EXPECT_EQ(iframe_url, iframe->GetLastCommittedURL());

  // Set up test manifest downloader that knows how to fake origin.
  const std::string method_name = "kylepay.test";
  SetDownloaderAndIgnorePortInOriginComparisonForTestingInFrame(
      {{method_name, &app_server_}}, iframe);

  EXPECT_EQ(
      "RangeError: Failed to construct 'PaymentRequest': "
      "https://kylepay.test/webpay payment method identifier violates "
      "Content Security Policy.",
      content::EvalJs(iframe, "checkCanMakePayment()"));

  EXPECT_TRUE(console_observer.messages().empty());
}

// Verify that a page's CSP can deny connections to a payment app's manifest
// files.
IN_PROC_BROWSER_TEST_F(IframeCspTest, PageCSPDeniesPayments) {
  NavigateTo("/csp/deny_csp.html");

  // The payment method identifier for an app that can be installed just in time
  // (JIT), unless CSP blocks connections to it.
  std::string payment_method =
      https_server()->GetURL("nickpay.test", "/nickpay.test/pay").spec();

  // The test page's CSP denies connections to all payment manifests.
  EXPECT_EQ(
      "RangeError: Failed to construct 'PaymentRequest': " + payment_method +
          " payment method identifier violates Content Security Policy.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("checkCanMakePayment($1)", payment_method)));
}

// Verify that CSP can deny redirects for payment method manifests.
IN_PROC_BROWSER_TEST_F(IframeCspTest, PageCSPDeniesRedirectedPaymentDownloads) {
  NavigateTo("/csp/deny_csp_after_redirect.html");

  // "https://test.example/redirect/pay" redirects to
  // "https://subdomain.test.example/redirect/destination/pay". CSP denies
  // access to the subdomain.
  std::string domain = "test.example";
  std::string subdomain = "subdomain." + domain;
  SetDownloaderAndIgnorePortInOriginComparisonForTesting(
      {{domain, https_server()}, {subdomain, https_server()}});
  std::string payment_method = "https://" + domain + "/redirect/pay";

  // The test page's CSP denies connections to the redirect destination.
  EXPECT_EQ(false, content::EvalJs(GetActiveWebContents(),
                                   content::JsReplace("checkCanMakePayment($1)",
                                                      payment_method)))
      << "Expected canMakePayment to fail due to CSP connect-src directive, "
         "but it succeeded.";
}

}  // namespace payments
