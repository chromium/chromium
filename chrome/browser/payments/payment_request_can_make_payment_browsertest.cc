// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/payments/personal_data_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {

class PaymentRequestCanMakePaymentTestBase : public PlatformBrowserTest,
                                             public PaymentRequestTestObserver {
 protected:
  // Various events that can be waited on by the EventWaiter.
  enum TestEvent : int {
    CAN_MAKE_PAYMENT_CALLED,
    CAN_MAKE_PAYMENT_RETURNED,
    HAS_ENROLLED_INSTRUMENT_CALLED,
    HAS_ENROLLED_INSTRUMENT_RETURNED,
    CONNECTION_TERMINATED,
    NOT_SUPPORTED_ERROR,
    ABORT_CALLED,
  };

  PaymentRequestCanMakePaymentTestBase() {
    payment_request_controller_.SetObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "a.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_->StartAcceptingConnections();

    Profile* profile = Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    personal_data_manager->SetSyncServiceForTest(&sync_service_);

    payment_request_controller_.SetUpOnMainThread();
  }

  void SetIncognito(bool is_incognito) {
    payment_request_controller_.SetIncognito(is_incognito);
  }

  void SetValidSsl(bool valid_ssl) {
    payment_request_controller_.SetValidSsl(valid_ssl);
  }

  void SetCanMakePaymentEnabledPref(bool can_make_payment) {
    payment_request_controller_.SetCanMakePaymentEnabledPref(can_make_payment);
  }

  void ExpectBodyContains(const std::vector<std::string>& expected_strings) {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string extract_contents_js =
        "(function() { "
        "window.domAutomationController.send(window.document.body.textContent);"
        " "
        "})()";
    std::string contents;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, extract_contents_js, &contents));
    for (const std::string& expected_string : expected_strings) {
      EXPECT_NE(std::string::npos, contents.find(expected_string))
          << "String \"" << expected_string
          << "\" is not present in the content \"" << contents << "\"";
    }
  }

  void AddAutofillProfile(const autofill::AutofillProfile& autofill_profile) {
    test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                             autofill_profile);
  }

  void AddCreditCard(const autofill::CreditCard& card) {
    test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateTo(const std::string& file_path) {
    EXPECT_TRUE(content::NavigateToURL(
        GetActiveWebContents(), https_server_->GetURL("a.com", file_path)));
  }

  // PaymentRequestTestObserver implementation.
  void OnCanMakePaymentCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CAN_MAKE_PAYMENT_CALLED);
  }
  void OnCanMakePaymentReturned() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CAN_MAKE_PAYMENT_RETURNED);
  }
  void OnHasEnrolledInstrumentCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::HAS_ENROLLED_INSTRUMENT_CALLED);
  }
  void OnHasEnrolledInstrumentReturned() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::HAS_ENROLLED_INSTRUMENT_RETURNED);
  }
  void OnNotSupportedError() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::NOT_SUPPORTED_ERROR);
  }
  void OnConnectionTerminated() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CONNECTION_TERMINATED);
  }
  void OnAbortCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::ABORT_CALLED);
  }

  void ResetEventWaiterForSequence(std::list<TestEvent> event_sequence) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<TestEvent>>(
        std::move(event_sequence));
  }
  void WaitForObservedEvent() { event_waiter_->Wait(); }

 private:
  PaymentRequestTestController payment_request_controller_;

  syncer::TestSyncService sync_service_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<autofill::EventWaiter<TestEvent>> event_waiter_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentTestBase);
};

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryTest() = default;

  void CallCanMakePayment() {
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument() {
    ResetEventWaiterForSequence({TestEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                                 TestEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
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
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Visa is required, and user has a visa instrument, but canMakePayment is
// disabled by user preference.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_SupportedButDisabled) {
  SetCanMakePaymentEnabledPref(false);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
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
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
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
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Pages without a valid SSL certificate always get "false" from
// .canMakePayment().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetValidSsl(false);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, user has a visa instrument, and user is in incognito
// mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Visa is required, and user doesn't have a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, user doesn't have a visa instrument and the user is in
// incognito mode. In this case canMakePayment returns false as in a normal
// profile.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
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
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       visa ? "buy();" : "other_buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(bool visa) {
    ResetEventWaiterForSequence({TestEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                                 TestEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
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
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaAppliesToDifferentMethods) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  // canMakePayment doesn't have query quota.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  // canMakePayment doesn't have query quota and is not affected by enrolled
  // instrument.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});
}

// canMakePayment should return result in incognito mode as in normal mode to
// avoid incognito mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaInIncognito) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");
  SetIncognito(true);

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  // canMakePayment doesn't have query quota.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  // Check hasEnrolledInstrument on a different method hits the query quota.
  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  // canMakePayment doesn't have query quota and is not affected by enrolled
  // instrument.
  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});
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
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        "checkCanMakePayment(" + script_[check_for] + ");"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(CheckFor check_for) {
    ResetEventWaiterForSequence({TestEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                                 TestEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
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
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"false"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  // This is considered a different method, so hasEnrolledInstrument() will be
  // throttled.
  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  // Add a visa card to the user profile.
  AddCreditCard(autofill::test::GetCreditCard());

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});
}

// canMakePayment() should return result as in normal mode to avoid incognito
// mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCardsInIncognito) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  SetIncognito(true);

  // User starts off without having a visa card.
  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"false"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  // This is considered a different method, so hasEnrolledInstrument() will be
  // throttled.
  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  // Add a visa card to the user profile.
  AddCreditCard(autofill::test::GetCreditCard());

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});
}

class PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuota
    : public PaymentRequestCanMakePaymentQueryPMITest {
 public:
  PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuota() {
    feature_list_.InitAndEnableFeature(
        features::kWebPaymentsPerMethodCanMakePaymentQuota);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the device does not have any payment apps installed, canMakePayment() and
// hasEnrolledInstrument() should return false for them.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuota,
                       QueryQuotaForPaymentApps) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
}

// If the device does not have any payment apps installed,
// hasEnrolledInstrument() queries for both payment apps and basic-card depend
// only on what cards the user has on file.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentAppsAndCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});
}

class
    PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuotaAndServiceWorkerPayment
    : public PaymentRequestCanMakePaymentQueryPMITest {
 public:
  PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuotaAndServiceWorkerPayment() {
    feature_list_.InitWithFeatures(
        /*enable_features=*/{::features::kServiceWorkerPaymentApps,
                             features::
                                 kWebPaymentsPerMethodCanMakePaymentQuota},
        /*disable_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Querying for payment apps in incognito returns result as normal mode to avoid
// incognito mode detection. Multiple queries for different apps are rejected
// with NotSupportedError to avoid user fingerprinting.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuotaAndServiceWorkerPayment,
    QueryQuotaForPaymentAppsInIncognitoMode) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  SetIncognito(true);

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
}

// Querying for both payment apps and autofill cards in incognito returns result
// as in normal mode to avoid incognito mode detection. Multiple queries for
// different payment methods are rejected with NotSupportedError to avoid user
// fingerprinting.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITestWithPaymentQuota,
                       NoQueryQuotaForPaymentAppsAndCardsInIncognito) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  SetIncognito(true);

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
}

}  // namespace payments
