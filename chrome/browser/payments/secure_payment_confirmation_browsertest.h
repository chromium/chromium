// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_
#define CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace base {
class CommandLine;
}

namespace payments {

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase,
      public WebDataServiceConsumer {
 public:
  SecurePaymentConfirmationTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSecurePaymentConfirmation,
                              features::kSecurePaymentConfirmationDebug},
        /*disabled_features=*/{});
  }

  // PaymentRequestPlatformBrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void OnAppListReady() override;
  void OnErrorDisplayed() override;

  // WebDataServiceConsumer
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  bool database_write_responded_ = false;
  bool confirm_payment_ = false;
  bool close_dialog_on_error_ = false;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_BROWSERTEST_H_
