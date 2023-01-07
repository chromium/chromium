// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using HasEnrolledInstrumentQueryQuotaTest =
    PaymentRequestPlatformBrowserTestBase;

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentQueryQuotaTest, NoFlags) {
  NavigateTo("a.com", "/has_enrolled_instrument.html");
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

}  // namespace
}  // namespace payments
