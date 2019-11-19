// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

class PaymentHandlerEnableDelegationsTest : public PlatformBrowserTest {
 public:
  PaymentHandlerEnableDelegationsTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PaymentHandlerEnableDelegationsTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(), https_server_.GetURL("/payment_handler.html")));
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  PaymentRequestTestController test_controller_;
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerEnableDelegationsTest);
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest, EnableDelegations) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(
      expected,
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(["
          "'shippingAddress', 'payerName', 'payerPhone', 'payerEmail'])"));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       InvalidDelegations) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));

  expected =
      "TypeError: Failed to execute 'enableDelegations' on 'PaymentManager': "
      "The provided value 'invalid_delegation' is not a valid enum value of "
      "type PaymentDelegation.";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['invalid_delegation'])"));
}

}  // namespace
}  // namespace payments
