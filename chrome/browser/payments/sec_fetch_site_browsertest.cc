// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

static constexpr char kPaymentMethod[] = "/";

class SecFetchSiteTest : public PaymentRequestPlatformBrowserTestBase {
 public:
  SecFetchSiteTest() = default;

  void SetUpOnMainThread() override {
    response_ = std::make_unique<net::test_server::ControllableHttpResponse>(
        https_server(), kPaymentMethod);
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

  GURL GetTestServerUrl(const std::string& hostname, const std::string& path) {
    return https_server()->GetURL(hostname, path);
  }

  std::string GetSecFetchSiteHeader() {
    response_->WaitForRequest();
    return response_->http_request()->headers.at("Sec-Fetch-Site");
  }

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> response_;
};

// When merchant https://a.com uses the payment method from https://b.com, the
// HTTP HEAD request has a "Sec-Fetch-Site: cross-site" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest,
                       CrossSitePaymentMethodManifestRequest) {
  NavigateTo("a.com", "/payment_request_creator.html");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("b.com", kPaymentMethod).spec())));
  EXPECT_EQ("cross-site", GetSecFetchSiteHeader());
}

// When merchant https://a.com uses the payment method from https://a.com, the
// HTTP HEAD request has a "Sec-Fetch-Site: same-origin" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest,
                       SameOriginPaymentMethodManifestRequest) {
  NavigateTo("a.com", "/payment_request_creator.html");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("a.com", kPaymentMethod).spec())));
  EXPECT_EQ("same-origin", GetSecFetchSiteHeader());
}

// When merchant https://x.a.com uses the payment method from https://y.a.com,
// the HTTP HEAD request has a "Sec-Fetch-Site: same-site" header.
IN_PROC_BROWSER_TEST_F(SecFetchSiteTest, SameSitePaymentMethodManifestRequest) {
  NavigateTo("x.a.com", "/payment_request_creator.html");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("createPaymentRequest($1)",
                         GetTestServerUrl("y.a.com", kPaymentMethod).spec())));
  EXPECT_EQ("same-site", GetSecFetchSiteHeader());
}

}  // namespace
}  // namespace payments
