// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/personal_data_manager_test_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/payments/core/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace payments {

class PaymentRequestCanMakePaymentTestBase
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentRequestCanMakePaymentTestBase() = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    Profile* profile = Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    personal_data_manager->SetSyncServiceForTest(&sync_service_);
  }

  void NavigateTo(const std::string& file_path) {
    PaymentRequestPlatformBrowserTestBase::NavigateTo("a.com", file_path);
  }

 private:
  syncer::TestSyncService sync_service_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentTestBase);
};

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryTest() = default;

  void CallCanMakePayment() {
    ResetEventWaiterForEventSequence(
        {TestEvent::kCanMakePaymentCalled, TestEvent::kCanMakePaymentReturned});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument() {
    ResetEventWaiterForEventSequence(
        {TestEvent::kHasEnrolledInstrumentCalled,
         TestEvent::kHasEnrolledInstrumentReturned});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       "hasEnrolledInstrument();"));
    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryTest);
};

// Visa is required, and user has a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("true");
}

// Visa is required, and user has a visa instrument, but canMakePayment is
// disabled by user preference.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_SupportedButDisabled) {
  test_controller()->SetCanMakePaymentEnabledPref(false);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("false");

  CallHasEnrolledInstrument();
  ExpectBodyContains("false");
}

class PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsDisabled
    : public PaymentRequestCanMakePaymentQueryTest {
 public:
  PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsDisabled() {
    feature_list_.InitAndDisableFeature(
        payments::features::kReturnGooglePayInBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is disabled.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsDisabled,
    CanMakePayment_Supported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::AutofillProfile billing_address = CreateAndAddAutofillProfile();
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(u"4111111111111111");  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("false");
}

class PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsEnabled
    : public PaymentRequestCanMakePaymentQueryTest {
 public:
  PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsEnabled() {
    feature_list_.InitAndEnableFeature(
        payments::features::kReturnGooglePayInBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is enabled.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentQueryTestWithGooglePayCardsEnabled,
    CanMakePayment_Supported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(u"4111111111111111");  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("true");
}

// Pages without a valid SSL certificate always get "false" from
// .canMakePayment().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
  WaitForObservedEvent();
  ExpectBodyContains("false");
}

// Pages without a valid SSL certificate always get NotSupported error from
// .show().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest, Show_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "show();"));
  WaitForObservedEvent();
  ExpectBodyContains("NotSupportedError: Invalid SSL certificate");
}

// Pages without a valid SSL certificate always get "false" from
// .hasEnrolledInstrument().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       HasEnrolledInstrument_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "hasEnrolledInstrument();"));
  WaitForObservedEvent();
  ExpectBodyContains("false");
}

// Visa is required, user has a visa instrument, and user is in incognito
// mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("true");
}

// Visa is required, and user doesn't have a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("false");
}

// Visa is required, user doesn't have a visa instrument and the user is in
// incognito mode. In this case canMakePayment returns false as in a normal
// profile.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains("true");

  CallHasEnrolledInstrument();
  ExpectBodyContains("false");
}

class PaymentRequestCanMakePaymentQueryCCTest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryCCTest() = default;

  // If |visa| is true, then the method data is:
  //
  //   [{supportedMethods: ['visa']}]
  //
  // If |visa| is false, then the method data is:
  //
  //   [{supportedMethods: ['mastercard']}]
  void CallCanMakePayment(bool visa) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kCanMakePaymentCalled, TestEvent::kCanMakePaymentReturned});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       visa ? "buy();" : "other_buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(bool visa) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kHasEnrolledInstrumentCalled,
         TestEvent::kHasEnrolledInstrumentReturned});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(), visa ? "hasEnrolledInstrument('visa');"
                                     : "hasEnrolledInstrument('mastercard');"));
    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryCCTest);
};

// Test that repeated canMakePayment and hasEnrolledInstrument queries are
// allowed when the payment method specifics don't change.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaDoesNotApplyToSameMethod) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("false");

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("false");

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("true");

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("true");
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaAppliesToDifferentMethods) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("false");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains("NotAllowedError");

  // canMakePayment doesn't have query quota.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("true");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains("NotAllowedError");

  // canMakePayment doesn't have query quota and is not affected by enrolled
  // instrument.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");
}

// canMakePayment should return result in incognito mode as in normal mode to
// avoid incognito mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaInIncognito) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");
  test_controller()->SetOffTheRecord(true);

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("false");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains("NotAllowedError");

  // canMakePayment doesn't have query quota.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains("true");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains("true");

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains("NotAllowedError");

  // canMakePayment doesn't have query quota and is not affected by enrolled
  // instrument.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains("true");
}

class PaymentRequestCanMakePaymentQueryPMITest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  enum class CheckFor {
    BASIC_VISA,
    BASIC_CARD,
    ALICE_PAY,
    BOB_PAY,
    BOB_PAY_AND_BASIC_CARD,
    BOB_PAY_AND_VISA,
  };

  PaymentRequestCanMakePaymentQueryPMITest() {
    script_[CheckFor::BASIC_VISA] = "[basicVisaMethod]";
    script_[CheckFor::BASIC_CARD] = "[basicCardMethod]";
    script_[CheckFor::ALICE_PAY] = "[alicePayMethod]";
    script_[CheckFor::BOB_PAY] = "[bobPayMethod]";
    script_[CheckFor::BOB_PAY_AND_BASIC_CARD] =
        "[bobPayMethod, basicCardMethod]";
    script_[CheckFor::BOB_PAY_AND_VISA] = "[bobPayMethod, basicVisaMethod]";
  }

  void CallCanMakePayment(CheckFor check_for) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kCanMakePaymentCalled, TestEvent::kCanMakePaymentReturned});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        "checkCanMakePayment(" + script_[check_for] + ");"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(CheckFor check_for) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kHasEnrolledInstrumentCalled,
         TestEvent::kHasEnrolledInstrumentReturned});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        "checkHasEnrolledInstrument(" + script_[check_for] + ");"));
    WaitForObservedEvent();
  }

 private:
  base::flat_map<CheckFor, std::string> script_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryPMITest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  // User starts off without having a visa card.
  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains("false");

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  // This is considered a different method, so hasEnrolledInstrument() will be
  // throttled.
  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains("NotAllowedError");

  // Add a visa card to the user profile.
  AddCreditCard(autofill::test::GetCreditCard());

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");

  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains("NotAllowedError");
}

// canMakePayment() should return result as in normal mode to avoid incognito
// mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCardsInIncognito) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  test_controller()->SetOffTheRecord(true);

  // User starts off without having a visa card.
  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains("false");

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  // This is considered a different method, so hasEnrolledInstrument() will be
  // throttled.
  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains("NotAllowedError");

  // Add a visa card to the user profile.
  AddCreditCard(autofill::test::GetCreditCard());

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains("true");

  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains("NotAllowedError");
}

// If the device does not have any payment apps installed,
// hasEnrolledInstrument() queries for both payment apps and basic-card depend
// only on what cards the user has on file.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentAppsAndCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("false");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("NotAllowedError");

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("false");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("NotAllowedError");

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains("true");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("true");
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains("NotAllowedError");
}

}  // namespace payments
