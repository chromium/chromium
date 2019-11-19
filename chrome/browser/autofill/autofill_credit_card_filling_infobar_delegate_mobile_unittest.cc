// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillCreditCardFillingInfoBarDelegateMobileTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillCreditCardFillingInfoBarDelegateMobileTest()
      : infobar_callback_has_run_(false) {}
  ~AutofillCreditCardFillingInfoBarDelegateMobileTest() override {}

 protected:
  std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile>
  CreateDelegate();

  void AcceptInfoBarCallback() { infobar_callback_has_run_ = true; }

  bool infobar_callback_has_run_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardFillingInfoBarDelegateMobileTest);
};

std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile>
AutofillCreditCardFillingInfoBarDelegateMobileTest::CreateDelegate() {
  infobar_callback_has_run_ = false;
  CreditCard credit_card;
  std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile> delegate(
      new AutofillCreditCardFillingInfoBarDelegateMobile(
          credit_card,
          base::BindOnce(&AutofillCreditCardFillingInfoBarDelegateMobileTest::
                             AcceptInfoBarCallback,
                         base::Unretained(this))));
  delegate->set_was_shown();
  return delegate;
}

// Test that credit card infobar metrics are logged correctly.
TEST_F(AutofillCreditCardFillingInfoBarDelegateMobileTest, Metrics) {
  // Accept the infobar.
  {
    std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile> infobar(
        CreateDelegate());

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    EXPECT_TRUE(infobar_callback_has_run_);
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_ACCEPTED, 1);
    infobar.reset();
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Cancel the infobar.
  {
    std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile> infobar(
        CreateDelegate());

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Cancel());
    EXPECT_FALSE(infobar_callback_has_run_);
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_DENIED, 1);
    infobar.reset();
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile> infobar(
        CreateDelegate());

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    EXPECT_FALSE(infobar_callback_has_run_);
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_DENIED, 1);
    infobar.reset();
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<AutofillCreditCardFillingInfoBarDelegateMobile> infobar(
        CreateDelegate());

    base::HistogramTester histogram_tester;
    infobar.reset();
    EXPECT_FALSE(infobar_callback_has_run_);
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.CreditCardFillingInfoBar",
                                       AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

}  // namespace autofill
