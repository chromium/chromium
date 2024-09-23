// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using test::CreateTestCreditCardFormData;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Ref;
using ::testing::Return;

Matcher<Suggestion> EqualsSuggestionFields(const std::u16string& main_text,
                                           const std::u16string& minor_text,
                                           bool apply_deactivated_style) {
  return AllOf(
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(false))),
      Field(&Suggestion::minor_text,
            Suggestion::Text(minor_text, Suggestion::Text::IsPrimary(false))),
      Field(&Suggestion::apply_deactivated_style, apply_deactivated_style));
}

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : payments::TestPaymentsAutofillClient(client) {}
  MockPaymentsAutofillClient(const MockPaymentsAutofillClient&) = delete;
  MockPaymentsAutofillClient& operator=(const MockPaymentsAutofillClient&) =
      delete;
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callback),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              ((base::WeakPtr<autofill::TouchToFillDelegate> delegate),
               (base::span<const CreditCard> cards_to_suggest),
               (base::span<const Suggestion> suggestions)),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillIban,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const Iban> ibans_to_suggest),
              (override));
  MOCK_METHOD(void, HideTouchToFillPaymentMethod, (), (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards() {
    EXPECT_CALL(*this, ShowTouchToFillCreditCard)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const CreditCard> cards_to_suggest,
                         base::span<const Suggestion> suggestions) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillPaymentMethod).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForIbans() {
    EXPECT_CALL(*this, ShowTouchToFillIban)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const Iban> ibans_to_suggest) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillPaymentMethod).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

 private:
  base::WeakPtr<autofill::TouchToFillDelegate> captured_delegate_;
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    set_payments_autofill_client(
        std::make_unique<MockPaymentsAutofillClient>(this));
  }
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void, ShowAutofillSettings, (SuggestionType), (override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (SuggestionHidingReason reason),
              (override));
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(TestAutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerDetails& trigger_details),
              (override));
  MOCK_METHOD(void,
              AuthenticateThenFillCreditCardForm,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const AutofillTriggerDetails& trigger_details));
  MOCK_METHOD(void,
              DidShowSuggestions,
              (DenseSet<SuggestionType> shown_suggestion_types,
               const FormData& form,
               const FormFieldData& field),
              (override));
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const, override));
  MOCK_METHOD(AutofillField*,
              GetAutofillField,
              (const FormData& form, const FormFieldData& field));
};

class TouchToFillDelegateAndroidImplUnitTest : public testing::Test {
 public:
  TouchToFillDelegateAndroidImplUnitTest() {
    // Some date after in the 2000s because Autofill doesn't allow expiration
    // dates before 2000.
    task_environment_.AdvanceClock(base::Days(365 * 50));
  }

 protected:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.GetPersonalDataManager()->SetPrefService(
        autofill_client_.GetPrefs());
    autofill_driver_ = std::make_unique<TestAutofillDriver>(&autofill_client_);
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get());

    auto touch_to_fill_delegate =
        std::make_unique<TouchToFillDelegateAndroidImpl>(
            browser_autofill_manager_.get());
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    base::WeakPtr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_weak =
        touch_to_fill_delegate->GetWeakPtr();
    browser_autofill_manager_->set_touch_to_fill_delegate(
        std::move(touch_to_fill_delegate));

    // Default setup for successful `TryToShowTouchToFill`.
    ON_CALL(*browser_autofill_manager_, CanShowAutofillUi)
        .WillByDefault(Return(true));
    ON_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
        .WillByDefault(Return(true));
    ON_CALL(payments_autofill_client(), ShowTouchToFillIban)
        .WillByDefault(Return(true));
    // Calling HideTouchToFillPaymentMethod in production code leads to that
    // OnDismissed gets triggered (HideTouchToFillPaymentMethod calls
    // view->Hide() on java side, which in its turn triggers onDismissed). Here
    // we mock this call.
    ON_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod)
        .WillByDefault([delegate = touch_to_fill_delegate_weak]() -> void {
          if (delegate) {
            delegate->OnDismissed(/*dismissed_by_user=*/false);
          }
        });
    autofill::MockFastCheckoutClient* fast_checkout_client =
        static_cast<autofill::MockFastCheckoutClient*>(
            autofill_client_.GetFastCheckoutClient());
    ON_CALL(*fast_checkout_client, IsNotShownYet)
        .WillByDefault(testing::Return(true));
  }

  // Helper method to add the given `card` and create a card form.
  void ConfigureForCreditCards(const CreditCard& card) {
    form_ = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
    test_api(form_).field(0).set_is_focusable(true);
    autofill_client_.GetPersonalDataManager()
        ->payments_data_manager()
        .AddCreditCard(card);
  }

  // Helper method to add an IBAN and create an IBAN form.
  std::string ConfigureForIbans() {
    Iban iban;
    iban.set_value(std::u16string(test::kIbanValue16));
    std::string guid = autofill_client_.GetPersonalDataManager()
                           ->test_payments_data_manager()
                           .AddAsLocalIban(std::move(iban));
    form_ = test::CreateTestIbanFormData(/*value=*/"");
    test_api(form_).field(0).set_is_focusable(true);
    return guid;
  }

  void OnFormsSeen() {
    if (!browser_autofill_manager_->FindCachedFormById(form_.global_id())) {
      browser_autofill_manager_->OnFormsSeen({form_}, {});
    }
  }

  void IntendsToShowTouchToFill(bool expected_success) {
    OnFormsSeen();
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IntendsToShowTouchToFill(
                  form_.global_id(), form_.fields()[0].global_id(), form_));
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillSuggestions(
                    SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);

    OnFormsSeen();
    EXPECT_EQ(expected_success, touch_to_fill_delegate_->TryToShowTouchToFill(
                                    form_, form_.fields()[0]));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return *static_cast<MockPaymentsAutofillClient*>(
        autofill_client_.GetPaymentsAutofillClient());
  }

  FormData form_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// Params of TouchToFillDelegateAndroidImplPaymentMethodUnitTest:
// -- bool IsCreditCard: Indicates whether the payment method is a credit card
//  or an IBAN.
class TouchToFillDelegateAndroidImplPaymentMethodUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableLocalIban);
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    if (IsCreditCard()) {
      ConfigureForCreditCards(test::GetCreditCard());
    } else {
      ConfigureForIbans();
    }
  }

  bool IsCreditCard() const { return GetParam(); }

  std::string GetTriggerOutcomeHistogramName() {
    return IsCreditCard() ? kUmaTouchToFillCreditCardTriggerOutcome
                          : kUmaTouchToFillIbanTriggerOutcome;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
                         testing::Bool());

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForInvalidForm) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  browser_autofill_manager_->ClearFormStructures();

  EXPECT_EQ(false, touch_to_fill_delegate_->TryToShowTouchToFill(
                       form_, form_.fields()[0]));
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfNotSpecificField) {
  test_api(form_).Insert(
      0, test::CreateTestFormField("Arbitrary", "arbitrary", "",
                                   FormControlType::kInputText));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       HideTouchToFillDoesNothingIfNotShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod)
      .Times(0);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       HideTouchToFillHidesIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       ResetHidesTouchToFillIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  touch_to_fill_delegate_->Reset();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       ResetAllowsShowingTouchToFillAgain) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();
  TryToShowTouchToFill(/*expected_success=*/false);

  touch_to_fill_delegate_->Reset();
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       OnDismissSetsTouchToFillToNotShowingState) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(false);

  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownBefore) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  EXPECT_CALL(autofill_client_,
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownCurrently) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_,
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(
      touch_to_fill_delegate_->TryToShowTouchToFill(form_, form_.fields()[0]));
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillPaymentMethodSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(*browser_autofill_manager_, DidShowSuggestions);
  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfWasShown) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kShownBefore, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfFieldIsNotFocusable) {
  test_api(form_).field(0).set_is_focusable(false);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  test_api(form_).field(0).set_value(u"Initial value");

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfNoPaymentMethodsOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearAllLocalData();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*browser_autofill_manager_, CanShowAutofillUi)
      .WillRepeatedly(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kCannotShowAutofillUi, 1);
}

class TouchToFillDelegateAndroidImplCreditCardUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 public:
  TouchToFillDelegateAndroidImplCreditCardUnitTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableVcnGrayOutForMerchantOptOut,
        /*enabled=*/false);
  }

  static std::vector<CreditCard> GetCardsToSuggest(
      std::vector<CreditCard*> credit_cards) {
    std::vector<CreditCard> cards_to_suggest;
    cards_to_suggest.reserve(credit_cards.size());
    for (const CreditCard* card : credit_cards) {
      cards_to_suggest.push_back(*card);
    }
    return cards_to_suggest;
  }

 protected:
  void SetUp() override {
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForCreditCards(test::GetCreditCard());
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsForIncompleteForm) {
  // Erase expiration month and expiration year fields.
  ASSERT_EQ(form_.fields()[2].name(), u"ccmonth");
  test_api(form_).Remove(2);
  ASSERT_EQ(form_.fields()[2].name(), u"ccyear");
  test_api(form_).Remove(2);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kIncompleteForm, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsForPrefilledCardNumber) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set credit card value.
  // TODO(crbug.com/40900766): Retrieve the card number field by name here.
  ASSERT_EQ(form_.fields()[1].name(), u"cardnumber");
  test_api(form_).field(1).set_value(u"411111111111");
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormAlreadyFilled, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillSucceedsForPrefilledYear) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set card expiration year.
  // TODO(crbug.com/40900766): Retrieve the card expiry year field by name here.
  ASSERT_EQ(form_.fields()[3].name(), u"ccyear");
  test_api(form_).field(3).set_value(u"2023");
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/true);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfFormIsNotSecure) {
  // Simulate non-secure form.
  form_ = test::CreateTestCreditCardFormData(/*is_https=*/false,
                                             /*use_month_type=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfClientIsNotSecure) {
  // Simulate non-secure client.
  autofill_client_.set_form_origin(GURL("http://example.com"));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillToleratesFormattingCharacters) {
  test_api(form_).field(0).set_value(u"____-____-____-____");

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfCardIsIncomplete) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_no_number = test::GetCreditCard();
  cc_no_number.SetNumber(u"");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_exp_date = test::GetCreditCard();
  cc_no_exp_date.SetExpirationMonth(0);
  cc_no_exp_date.SetExpirationYear(0);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_exp_date);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_name = test::GetCreditCard();
  cc_no_name.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_name);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 3);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfTheOnlyCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(test::GetMaskedServerCard());

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfFastCheckoutWasShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill::MockFastCheckoutClient* fast_checkout_client =
      static_cast<autofill::MockFastCheckoutClient*>(
          autofill_client_.GetFastCheckoutClient());
  EXPECT_CALL(*fast_checkout_client, IsNotShownYet).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFastCheckoutWasShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillSucceedsIfAtLestOneCardIsValid) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsExpiredCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(expired_card);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardsToSuggest();

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAreArray(GetCardsToSuggest(credit_cards)),
          ElementsAre(
              EqualsSuggestionFields(
                  credit_cards[0]->CardNameForAutofillDisplay(
                      credit_cards[0]->nickname()),
                  credit_cards[0]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false),
              EqualsSuggestionFields(
                  credit_cards[1]->CardNameForAutofillDisplay(
                      credit_cards[1]->nickname()),
                  credit_cards[1]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillDoesNotShowDisusedExpiredCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard disused_expired_card = test::GetExpiredCreditCard();
  credit_card.set_use_date(AutofillClock::Now());
  disused_expired_card.set_use_date(AutofillClock::Now() -
                                    kDisusedDataModelTimeDelta * 2);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(disused_expired_card);
  ASSERT_TRUE(credit_card.IsCompleteValidCard());
  ASSERT_FALSE(disused_expired_card.IsCompleteValidCard());
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAre(credit_card),
          ElementsAre(EqualsSuggestionFields(
              credit_card.CardNameForAutofillDisplay(credit_card.nickname()),
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*apply_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(
    TouchToFillDelegateAndroidImplCreditCardUnitTest,
    TryToShowTouchToFillDoesNotShowVirtualCardSuggestionsForOptedOutMerchants) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));

  // Since merchant has opted out of virtual cards and gray-out feature is
  // disabled, ‘apply_deactivated_style` property should be set to false for the
  // virtual card suggestion.
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAre(credit_card),
          ElementsAre(EqualsSuggestionFields(
              credit_card.CardNameForAutofillDisplay(credit_card.nickname()),
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*apply_deactivated_style=*/false))));
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsVirtualCardSuggestionsForEnrolledCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  CreditCard virtual_card = CreditCard::CreateVirtualCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(false));

  // Since the card is enrolled into the virtual cards feature, a virtual card
  // suggestion should be created and added before the real card.
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAreArray({virtual_card, credit_card}),
          ElementsAre(
              EqualsSuggestionFields(
                  virtual_card.CardNameForAutofillDisplay(
                      virtual_card.nickname()),
                  virtual_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false),
              EqualsSuggestionFields(
                  credit_card.CardNameForAutofillDisplay(
                      credit_card.nickname()),
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       SafelyHideTouchToFillInDtor) {
  payments_autofill_client()
      .ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

// Add one IBAN to the PDM and verify that IBAN is not shown for the credit
// card form.
TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       PassTheCreditCardsToTheClient) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddAsLocalIban(std::move(iban1));
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card1);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card2);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardsToSuggest();

  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAreArray(GetCardsToSuggest(credit_cards)),
          ElementsAre(
              EqualsSuggestionFields(
                  credit_cards[0]->CardNameForAutofillDisplay(
                      credit_cards[0]->nickname()),
                  credit_cards[0]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false),
              EqualsSuggestionFields(
                  credit_cards[1]->CardNameForAutofillDisplay(
                      credit_cards[1]->nickname()),
                  credit_cards[1]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ScanCreditCardIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);
  EXPECT_CALL(payments_autofill_client(), ScanCreditCard);
  touch_to_fill_delegate_->ScanCreditCard();

  CreditCard credit_card = autofill::test::GetCreditCard();
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewCreditCardForm);
  touch_to_fill_delegate_->OnCreditCardScanned(credit_card);
  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ShowCreditCardSettingsIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client_,
      ShowAutofillSettings(testing::Eq(SuggestionType::kManageCreditCard)));
  touch_to_fill_delegate_->ShowPaymentMethodSettings();

  ASSERT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionClosesTheSheet) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, AuthenticateThenFillCreditCardForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       VirtualCardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, AuthenticateThenFillCreditCardForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       AutofillUsedAfterTouchToFillDismissal) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  // Simulate that the form was autofilled by other means
  FormStructure submitted_form(form_);
  for (const std::unique_ptr<AutofillField>& field : submitted_form) {
    field->set_is_autofilled(true);
  }

  touch_to_fill_delegate_->LogMetricsAfterSubmission(submitted_form);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.TouchToFill.CreditCard.AutofillUsedAfterTouchToFillDismissal",
      true, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       IsFormPrefilledHandlesNullAutofillField) {
  // `IntendsToShowTouchToFill()` invokes `DryRun()` that checks if form_ is
  // prefilled. `IsFormPrefilled()` calls
  // BrowserAutofillManager::GetAutofillField(). This tests the scenario where
  // `GetAutofillField()` returns a nullptr does not crash.
  ON_CALL(*browser_autofill_manager_, GetAutofillField(_, _))
      .WillByDefault(Return(nullptr));
  IntendsToShowTouchToFill(/*expected_success=*/true);
}

class TouchToFillDelegateAndroidImplIbanUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableLocalIban);
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForIbans();
  }
};

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillIban)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

// Add one valid credit card to PDM and verify that credit card is not shown in
// IBAN form.
TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest, PassTheIbansToTheClient) {
  TestPaymentsDataManager& paydm =
      autofill_client_.GetPersonalDataManager()->test_payments_data_manager();
  paydm.ClearAllLocalData();
  paydm.AddCreditCard(autofill::test::GetCreditCard());
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  paydm.AddAsLocalIban(std::move(iban1));
  Iban iban2;
  iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_2)));
  paydm.AddAsLocalIban(std::move(iban2));
  std::vector<Iban> ibans = paydm.GetOrderedIbansToSuggest();

  EXPECT_CALL(payments_autofill_client(),
              ShowTouchToFillIban(_, ElementsAreArray(ibans)));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillIban)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillFailsIfFlagOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSkipAndroidBottomSheetForIban);
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearAllLocalData();
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddAsLocalIban(std::move(iban));

  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillIban).Times(0);
  TryToShowTouchToFill(/*expected_success=*/false);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       SafelyHideTouchToFillInDtor) {
  payments_autofill_client()
      .ExpectDelegateWeakPtrFromShowInvalidatedOnHideForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       IbanSelectionClosesTheSheet) {
  std::string guid = ConfigureForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  touch_to_fill_delegate_->IbanSuggestionSelected(Iban::Guid(guid));
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       LocalIbanSelectionFillsIbanForm) {
  std::string guid = ConfigureForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*payments_autofill_client().GetIbanAccessManager(), FetchValue);
  touch_to_fill_delegate_->IbanSuggestionSelected(Iban::Guid(guid));
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       ServerIbanSelectionFillsIbanForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .SetSyncingForTest(true);
  std::string guid = ConfigureForIbans();
  // Add a server IBAN with a different instrument_id and verify `FetchValue`
  // is not triggered.
  long instrument_id = 123245678L;
  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(instrument_id));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddServerIban(server_iban);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*payments_autofill_client().GetIbanAccessManager(), FetchValue);
  touch_to_fill_delegate_->IbanSuggestionSelected(
      Iban::InstrumentId(instrument_id));
}

class TouchToFillDelegateAndroidImplVcnGrayOutForMerchantOptOutUnitTest
    : public TouchToFillDelegateAndroidImplCreditCardUnitTest {
 public:
  TouchToFillDelegateAndroidImplVcnGrayOutForMerchantOptOutUnitTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableVcnGrayOutForMerchantOptOut,
        /*enabled=*/true);
    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client_.GetAutofillOptimizationGuide()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(true));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchToFillDelegateAndroidImplVcnGrayOutForMerchantOptOutUnitTest,
       TryToShowTouchToFillWithVirtualCardSuggestionsForOptedOutMerchants) {
  CreditCard credit_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  credit_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  CreditCard virtual_card = CreditCard::CreateVirtualCard(credit_card);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  // Since VCN gray-out feature is active, the `apply_deactivated_style`
  // property should be true for the virtual card suggestion. However, the
  // `apply_deactivated_style` property should be set to false for the
  // associated real credit card suggestion.
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAreArray({virtual_card, credit_card}),
          ElementsAre(
              EqualsSuggestionFields(
                  virtual_card.CardNameForAutofillDisplay(
                      virtual_card.nickname()),
                  virtual_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/true),
              EqualsSuggestionFields(
                  credit_card.CardNameForAutofillDisplay(
                      credit_card.nickname()),
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*apply_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

}  // namespace
}  // namespace autofill
