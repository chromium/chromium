// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using PaymentMethodRedirectTest = PaymentRequestPlatformBrowserTestBase;

// Verify that a payment method identifier can be an HTTP 301 permanent redirect
// to its subdomain.
//
// Original payment method being used:
//     https://test.example/redirect/pay
//
// Redirect destination:
//     https://subdomain.test.example/redirect/destination/pay
IN_PROC_BROWSER_TEST_F(PaymentMethodRedirectTest, Redirect) {
  NavigateTo("/redirect/redirect.html");

  std::string domain = "test.example";
  std::string subdomain = "subdomain." + domain;
  SetDownloaderAndIgnorePortInOriginComparisonForTesting(
      {{domain, https_server()}, {subdomain, https_server()}});
  std::string payment_method = "https://" + domain + "/redirect/pay";

  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(),
                                  content::JsReplace("checkCanMakePayment($1)",
                                                     payment_method)));
}

}  // namespace
}  // namespace payments
