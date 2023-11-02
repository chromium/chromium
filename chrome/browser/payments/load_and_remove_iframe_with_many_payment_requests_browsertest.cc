// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class LoadAndRemoveIframeWithManyPaymentRequestsTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  LoadAndRemoveIframeWithManyPaymentRequestsTest() {
    // Enable the browser-side feature flag as it's disabled by default on
    // non-origin trial platforms.
    feature_list_.InitAndEnableFeature(features::kSecurePaymentConfirmation);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

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

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoadAndRemoveIframeWithManyPaymentRequestsTest,
                       CrossOriginNoCrash) {
  RunTest(/*iframe_hostname=*/"b.com");
}

IN_PROC_BROWSER_TEST_F(LoadAndRemoveIframeWithManyPaymentRequestsTest,
                       SameOriginNoCrash) {
  RunTest(/*iframe_hostname=*/"a.com");
}

}  // namespace
}  // namespace payments
