// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_delegate_android_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::NiceMock;
using testing::Pointee;
using testing::Ref;
using testing::Return;

namespace autofill {

namespace {

class MockFastCheckoutClient : public FastCheckoutClient {
 public:
  MockFastCheckoutClient() = default;
  ~MockFastCheckoutClient() override = default;
  MOCK_METHOD(bool,
              TryToStart,
              (const GURL&,
               const autofill::FormData&,
               const autofill::FormFieldData&,
               base::WeakPtr<autofill::AutofillManager>),
              (override));
  MOCK_METHOD(void, Stop, (bool), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(void, OnNavigation, (const GURL&, bool), (override));
  MOCK_METHOD(bool,
              IsSupported,
              (const autofill::FormData&,
               const autofill::FormFieldData&,
               const autofill::AutofillManager&),
              (const override));
  MOCK_METHOD(bool, IsNotShownYet, (), (const, override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callback),
              (override));
  MOCK_METHOD(bool, IsTouchToFillCreditCardSupported, (), (override));
  MOCK_METHOD(void, ShowAutofillSettings, (PopupType popup_type), (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const CreditCard> cards_to_suggest),
              (override));
  MOCK_METHOD(void, HideTouchToFillCreditCard, (), (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHide() {
    EXPECT_CALL(*this, ShowTouchToFillCreditCard)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const CreditCard> cards_to_suggest) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillCreditCard).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

 private:
  base::WeakPtr<autofill::TouchToFillDelegate> captured_delegate_;
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  MockBrowserAutofillManager(TestAutofillDriver* driver,
                             TestAutofillClient* client)
      : TestBrowserAutofillManager(driver, client) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(PopupType,
              GetPopupType,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (mojom::RendererFormDataAction action,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard* credit_card,
               const AutofillTriggerSource trigger_source));
  MOCK_METHOD(void,
              FillOrPreviewVirtualCardInformation,
              (mojom::RendererFormDataAction action,
               const std::string& guid,
               const FormData& form,
               const FormFieldData& field,
               const AutofillTriggerSource trigger_source));
  MOCK_METHOD(void,
              DidShowSuggestions,
              (bool has_autofill_suggestions,
               const FormData& form,
               const FormFieldData& field),
              (override));
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const, override));
};

}  // namespace

class TouchToFillDelegateAndroidImplUnitTest : public testing::Test {
 public:
  TouchToFillDelegateAndroidImplUnitTest() {
    // Some date after in the 2000s because Autofill doesn't allow expiration
    // dates before 2000.
    task_environment_.AdvanceClock(base::Days(365 * 50));
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

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.GetPersonalDataManager()->SetPrefService(
        autofill_client_.GetPrefs());
    autofill_driver_ = std::make_unique<TestAutofillDriver>();
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &autofill_client_);

    auto touch_to_fill_delegate =
        std::make_unique<TouchToFillDelegateAndroidImpl>(
            browser_autofill_manager_.get());
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    base::WeakPtr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_weak =
        touch_to_fill_delegate->GetWeakPtr();
    browser_autofill_manager_->set_touch_to_fill_delegate(
        std::move(touch_to_fill_delegate));

    // Default setup for successful `TryToShowTouchToFill`.
    autofill_client_.GetPersonalDataManager()->AddCreditCard(
        test::GetCreditCard());
    ON_CALL(*browser_autofill_manager_, GetPopupType(_, _))
        .WillByDefault(Return(PopupType::kCreditCards));
    ON_CALL(autofill_client_, IsTouchToFillCreditCardSupported)
        .WillByDefault(Return(true));
    ON_CALL(*browser_autofill_manager_, CanShowAutofillUi)
        .WillByDefault(Return(true));
    ON_CALL(autofill_client_, ShowTouchToFillCreditCard)
        .WillByDefault(Return(true));
    // Calling HideTouchToFillCreditCard in production code leads to that
    // OnDismissed gets triggered (HideTouchToFillCreditCard calls view->Hide()
    // on java side, which in its turn triggers onDismissed). Here we mock this
    // call.
    ON_CALL(autofill_client_, HideTouchToFillCreditCard)
        .WillByDefault([delegate = touch_to_fill_delegate_weak]() -> void {
          if (delegate) {
            delegate->OnDismissed(/*dismissed_by_user=*/false);
          }
        });
    MockFastCheckoutClient* fast_checkout_client =
        static_cast<MockFastCheckoutClient*>(
            autofill_client_.GetFastCheckoutClient());
    ON_CALL(*fast_checkout_client, IsNotShownYet)
        .WillByDefault(testing::Return(true));

    test::CreateTestCreditCardFormData(&form_, /*is_https=*/true,
                                       /*use_month_type=*/false);
    form_.fields[0].is_focusable = true;
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillPopup(
                    PopupHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);

    if (!browser_autofill_manager_->FindCachedFormById(form_.global_id())) {
      browser_autofill_manager_->OnFormsSeen({form_}, {});
    }
    EXPECT_EQ(expected_success, touch_to_fill_delegate_->TryToShowTouchToFill(
                                    form_, form_.fields[0]));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  FormData form_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchToFillDelegateAndroidImplUnitTest, TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(*browser_autofill_manager_, DidShowSuggestions);
  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfNotCreditCardField) {
  {
    FormFieldData field;
    test::CreateTestFormField("Arbitrary", "arbitrary", "", "text", &field);
    form_.fields.insert(form_.fields.begin(), field);
  }
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsForIncompleteForm) {
  // Erase expiration month and expiration year fields.
  ASSERT_EQ(form_.fields[2].name, u"ccmonth");
  form_.fields.erase(form_.fields.begin() + 2);
  ASSERT_EQ(form_.fields[2].name, u"ccyear");
  form_.fields.erase(form_.fields.begin() + 2);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kIncompleteForm, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsForPrefilledCardNumber) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set credit card value.
  // TODO(crbug/1428904): Retrieve the card number field by name here.
  ASSERT_EQ(form_.fields[1].name, u"cardnumber");
  form_.fields[1].value = u"411111111111";
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFormAlreadyFilled, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillSucceedsForPrefilledYear) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set card expiration year.
  // TODO(crbug/1428904): Retrieve the card expiry year field by name here.
  ASSERT_EQ(form_.fields[3].name, u"ccyear");
  form_.fields[3].value = u"2023";
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/true);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfNotSupported) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, IsTouchToFillCreditCardSupported)
      .WillRepeatedly(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfFormIsNotSecure) {
  // Simulate non-secure form.
  test::CreateTestCreditCardFormData(&form_, /*is_https=*/false,
                                     /*use_month_type=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfClientIsNotSecure) {
  // Simulate non-secure client.
  autofill_client_.set_form_origin(GURL("http://example.com"));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfShownBefore) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  EXPECT_CALL(
      autofill_client_,
      HideAutofillPopup(PopupHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfShownCurrently) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client_,
      HideAutofillPopup(PopupHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(
      touch_to_fill_delegate_->TryToShowTouchToFill(form_, form_.fields[0]));
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfWasShown) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShownBefore, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfFieldIsNotFocusable) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  form_.fields[0].is_focusable = false;

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  form_.fields[0].value = u"Initial value";

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillToleratesFormattingCharacters) {
  form_.fields[0].value = u"____-____-____-____";

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfNoCardsOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfCardIsIncomplete) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard cc_no_number = test::GetCreditCard();
  cc_no_number.SetNumber(u"");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_exp_date = test::GetCreditCard();
  cc_no_exp_date.SetExpirationMonth(0);
  cc_no_exp_date.SetExpirationYear(0);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_exp_date);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_name = test::GetCreditCard();
  cc_no_name.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_name);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 3);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfTheOnlyCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetMaskedServerCard());

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*browser_autofill_manager_, CanShowAutofillUi)
      .WillRepeatedly(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kCannotShowAutofillUi, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillFailsIfFastCheckoutWasShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kFastCheckout);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  MockFastCheckoutClient* fast_checkout_client =
      static_cast<MockFastCheckoutClient*>(
          autofill_client_.GetFastCheckoutClient());
  EXPECT_CALL(*fast_checkout_client, IsNotShownYet).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFastCheckoutWasShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillSucceedsIfAtLestOneCardIsValid) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillShowsExpiredCards) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(expired_card);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()->GetCreditCardsToSuggest();

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(
                  _, ElementsAreArray(GetCardsToSuggest(credit_cards))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       TryToShowTouchToFillDoesNotShowDisusedExpiredCards) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard disused_expired_card = test::GetExpiredCreditCard();
  credit_card.set_use_date(AutofillClock::Now());
  disused_expired_card.set_use_date(AutofillClock::Now() -
                                    kDisusedDataModelTimeDelta * 2);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      disused_expired_card);
  ASSERT_TRUE(credit_card.IsCompleteValidCard());
  ASSERT_FALSE(disused_expired_card.IsCompleteValidCard());
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAre(credit_card)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(
    TouchToFillDelegateAndroidImplUnitTest,
    TryToShowTouchToFillShowsVirtualCardSuggestionsForEnrolledCardsWhenEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillVirtualCardsOnTouchToFillAndroid);
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  // Since the card is enrolled into virtual card number, and showing virtual
  // cards is enabled, a virtual card suggestion should be created and added
  // before the real card.
  EXPECT_CALL(
      autofill_client_,
      ShowTouchToFillCreditCard(
          _, ElementsAreArray(
                 {CreditCard::CreateVirtualCard(credit_card), credit_card})));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(
    TouchToFillDelegateAndroidImplUnitTest,
    TryToShowTouchToFillDoesNotShowVirtualCardSuggestionsForEnrolledCardsWhenDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillVirtualCardsOnTouchToFillAndroid);
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  // Since showing virtual cards is disabled, no virtual card suggestion is
  // shown for virtual card number enrolled card.
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAre(credit_card)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       HideTouchToFillDoesNothingIfNotShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(0);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, HideTouchToFillHidesIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, ResetHidesTouchToFillIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->Reset();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       ResetAllowsShowingTouchToFillAgain) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();
  TryToShowTouchToFill(/*expected_success=*/false);

  touch_to_fill_delegate_->Reset();
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, SafelyHideTouchToFillInDtor) {
  autofill_client_.ExpectDelegateWeakPtrFromShowInvalidatedOnHide();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       OnDismissSetsTouchToFillToNotShowingState) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(false);

  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, PassTheCreditCardsToTheClient) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card1);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card2);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()->GetCreditCardsToSuggest();

  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(
                  _, ElementsAreArray(GetCardsToSuggest(credit_cards))));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, ScanCreditCardIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);
  EXPECT_CALL(autofill_client_, ScanCreditCard);
  touch_to_fill_delegate_->ScanCreditCard();

  CreditCard credit_card = autofill::test::GetCreditCard();
  EXPECT_CALL(*browser_autofill_manager_, FillCreditCardFormImpl);
  touch_to_fill_delegate_->OnCreditCardScanned(credit_card);
  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, ShowCreditCardSettingsIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_,
              ShowAutofillSettings(testing::Eq(PopupType::kCreditCards)));
  touch_to_fill_delegate_->ShowCreditCardSettings();

  ASSERT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, CardSelectionClosesTheSheet) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->SuggestionSelected(credit_card.server_id(), false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest, CardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewCreditCardForm);
  touch_to_fill_delegate_->SuggestionSelected(credit_card.server_id(), false);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       VirtualCardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewVirtualCardInformation);
  touch_to_fill_delegate_->SuggestionSelected(credit_card.server_id(), true);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       AutofillUsedAfterTouchToFillDismissal) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  // Simulate that the form was autofilled by other means
  FormStructure submitted_form(form_);
  for (const std::unique_ptr<AutofillField>& field : submitted_form) {
    field->is_autofilled = true;
  }

  touch_to_fill_delegate_->LogMetricsAfterSubmission(submitted_form);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.TouchToFill.CreditCard.AutofillUsedAfterTouchToFillDismissal",
      true, 1);
}

}  // namespace autofill
