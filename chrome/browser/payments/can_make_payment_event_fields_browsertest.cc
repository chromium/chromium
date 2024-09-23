// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace payments {

class CanMakePaymentEventFieldsTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  CanMakePaymentEventFieldsTest() = default;
  ~CanMakePaymentEventFieldsTest() override = default;

  bool GetValueOf(const std::string& js) {
    return content::EvalJs(GetActiveWebContents(), js).ExtractBool();
  }
};

IN_PROC_BROWSER_TEST_F(CanMakePaymentEventFieldsTest, VerifyFields) {
  std::string path = "/can_make_payment_event_fields";
  NavigateTo("a.com", path + "/test.html");

  std::string method =
      https_server()->GetURL("b.com", path + "/app/pay").spec();
  std::string invoke = content::JsReplace("invokePaymentRequest($1)", method);

  // Install the payment handler just-in-time, which does not invoke the
  // "canmakepayment" event.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), invoke));

  // Invoke the installed payment handler, so its "canmakepayment" event is
  // invoked.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), invoke));

  EXPECT_FALSE(GetValueOf("details.ifTopOrigin"));
  EXPECT_FALSE(GetValueOf("details.ifPaymentRequestOrigin"));
  EXPECT_FALSE(GetValueOf("details.ifMethodData"));
  EXPECT_FALSE(GetValueOf("details.ifModifiers"));

  EXPECT_TRUE(GetValueOf("details.emptyTopOrigin"));
  EXPECT_TRUE(GetValueOf("details.emptyPaymentRequestOrigin"));
  EXPECT_TRUE(GetValueOf("details.emptyMethodData"));
  EXPECT_TRUE(GetValueOf("details.emptyModifiers"));

  EXPECT_FALSE(GetValueOf("details.definedTopOrigin"));
  EXPECT_FALSE(GetValueOf("details.definedPaymentRequestOrigin"));
  EXPECT_FALSE(GetValueOf("details.definedMethodData"));
  EXPECT_FALSE(GetValueOf("details.definedModifiers"));

  EXPECT_FALSE(GetValueOf("details.inTopOrigin"));
  EXPECT_FALSE(GetValueOf("details.inPaymentRequestOrigin"));
  EXPECT_FALSE(GetValueOf("details.inMethodData"));
  EXPECT_FALSE(GetValueOf("details.inModifiers"));
}

}  // namespace payments
