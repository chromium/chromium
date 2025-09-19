// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_
#define CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_

#include <set>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "components/webdata/common/web_data_service_base.h"

class WDTypedResult;

namespace base {
class CommandLine;
}

namespace payments {

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  SecurePaymentConfirmationTest();
  ~SecurePaymentConfirmationTest() override;

  // PaymentRequestPlatformBrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void OnAppListReady() override;
  void OnErrorDisplayed() override;

  // Should be called to mock out that an ongoing database call has completed.
  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result);

  // Verify that the given set of JourneyLogger::Event2 `events` were logged by
  // JourneyLogger `count` times.
  void ExpectEvent2Histogram(std::set<JourneyLogger::Event2> events,
                             int count = 1);

  // Returns the generic WebAuthn error message, to compare against SPC output.
  static std::string GetWebAuthnErrorMessage();

  // Returns the cancel error message when the user closes the SPC dialog, to
  // compare against SPC output.
  static std::string GetCancelErrorMessage();

  bool database_write_responded_ = false;
  bool confirm_payment_ = false;
  bool close_dialog_on_error_ = false;
  bool accept_dialog_on_error_ = false;

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<SecurePaymentConfirmationTest> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_
