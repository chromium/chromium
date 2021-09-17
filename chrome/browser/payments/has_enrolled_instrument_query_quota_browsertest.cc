// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class HasEnrolledInstrumentQueryQuotaTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  HasEnrolledInstrumentQueryQuotaTest() = default;

  HasEnrolledInstrumentQueryQuotaTest(
      const HasEnrolledInstrumentQueryQuotaTest&) = delete;
  HasEnrolledInstrumentQueryQuotaTest& operator=(
      const HasEnrolledInstrumentQueryQuotaTest&) = delete;

  ~HasEnrolledInstrumentQueryQuotaTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    // Cannot use the default localhost hostname, because Chrome turns off the
    // quota for localhost and file:/// scheme to ease web development.
    NavigateTo("a.com", "/has_enrolled_instrument.html");
  }
};

class HasEnrolledInstrumentQueryQuotaTestNoFlags
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestNoFlags() {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kStrictHasEnrolledAutofillInstrument});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options do not trigger query quota when the strict autofill data
// check is disabled. Per-method query quota is also disabled in this test.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentQueryQuotaTestNoFlags, NoFlags) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

class HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options trigger query quota for Basic Card when the strict autofill
// data check is enabled. Per-method query quota is disabled in this test.
IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck,
    StrictAutofillDataCheck) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ("NotAllowedError: Exceeded query quota for hasEnrolledInstrument",
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

}  // namespace
}  // namespace payments
