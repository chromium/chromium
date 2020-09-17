// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

// TODO(crbug.com/1129573): fix flakiness and reenable
#if defined(OS_MAC)
#define MAYBE_LoadAndRemoveIframeWithManyPaymentRequestsTest \
  DISABLED_LoadAndRemoveIframeWithManyPaymentRequestsTest
#else
#define MAYBE_LoadAndRemoveIframeWithManyPaymentRequestsTest \
  LoadAndRemoveIframeWithManyPaymentRequestsTest
#endif

class MAYBE_LoadAndRemoveIframeWithManyPaymentRequestsTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void RunTest(const std::string& iframe_hostname) {
    NavigateTo("a.com", "/load_and_remove_iframe.html");

    // EvalJs waits for JavaScript promise to resolve.
    EXPECT_EQ("success",
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace(
                                  "loadAndRemoveIframe($1, /*timeout=*/100);",
                                  https_server()
                                      ->GetURL(iframe_hostname,
                                               "/create_many_requests.html")
                                      .spec())));
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_LoadAndRemoveIframeWithManyPaymentRequestsTest,
                       CrossOriginNoCrash) {
  RunTest(/*iframe_hostname=*/"b.com");
}

IN_PROC_BROWSER_TEST_F(MAYBE_LoadAndRemoveIframeWithManyPaymentRequestsTest,
                       SameOriginNoCrash) {
  RunTest(/*iframe_hostname=*/"a.com");
}

}  // namespace
}  // namespace payments
