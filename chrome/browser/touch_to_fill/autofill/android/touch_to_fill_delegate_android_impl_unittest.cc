// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
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
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;

Matcher<Suggestion> EqualsSuggestionFields(const std::u16string& main_text,
                                           const std::u16string& minor_text,
                                           bool has_deactivated_style) {
  return AllOf(
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(false))),
      Field(&Suggestion::minor_texts,
            std::vector<Suggestion::Text>{Suggestion::Text(minor_text)}),
      Property(&Suggestion::HasDeactivatedStyle, has_deactivated_style));
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
               (base::span<const Suggestion> suggestions)),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillIban,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const Iban> ibans_to_suggest),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillLoyaltyCard,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               std::vector<LoyaltyCard> loyalty_cards_to_suggest),
              (override));
  MOCK_METHOD(void, HideTouchToFillPaymentMethod, (), (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards() {
    EXPECT_CALL(*this, ShowTouchToFillCreditCard)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const Suggestion> suggestions) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillPaymentMethod).WillOnce([this] {
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
    EXPECT_CALL(*this, HideTouchToFillPaymentMethod).WillOnce([this] {
      EXPECT_FALSE(captured_delegate_);
    });
  }

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForLoyaltyCards() {
    EXPECT_CALL(*this, ShowTouchToFillLoyaltyCard)
        .WillOnce(
            [this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                   base::span<const LoyaltyCard> loyalty_cards_to_suggest) {
              captured_delegate_ = delegate;
              return true;
            });
    EXPECT_CALL(*this, HideTouchToFillPaymentMethod).WillOnce([this] {
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
              FillOrPreviewForm,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FieldGlobalId& field_id,
               const FillingPayload& filling_payload,
               AutofillTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewField,
              (mojom::ActionPersistence action_persistence,
               mojom::FieldActionType action_type,
               const FormData& form,
               const FormFieldData& field,
               const std::u16string& value,
               SuggestionType type,
               std::optional<FieldType> field_type),
              (override));
  MOCK_METHOD(void,
              DidShowSuggestions,
              (base::span<const Suggestion> suggestions,
               const FormData& form,
               const FieldGlobalId& field_id,
               AutofillExternalDelegate::UpdateSuggestionsCallback
                   update_suggestions_callback),
              (override));
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const, override));
  MOCK_METHOD(AutofillField*,
              GetAutofillField,
              (const FormData& form, const FormFieldData& field));
  MOCK_METHOD(void,
              LogAndRecordLoyaltyCardFill,
              (const LoyaltyCard&, const FormGlobalId&, const FieldGlobalId&),
              (override));
};

class TouchToFillDelegateAndroidImplUnitTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<
          NiceMock<MockAutofillClient>,
          TestAutofillDriver,
          NiceMock<MockBrowserAutofillManager>,
          MockPaymentsAutofillClient> {
 public:
  TouchToFillDelegateAndroidImplUnitTest() {
    features_.InitWithFeatures(
        {features::kAutofillEnableLoyaltyCardsFilling,
         features::kAutofillEnableEmailOrLoyaltyCardsFilling,
         features::kAutofillEnableBuyNowPayLaterSyncing},
        {});
    // Some date after in the 2000s because Autofill doesn't allow expiration
    // dates before 2000.
    task_environment_.AdvanceClock(base::Days(365 * 50));
  }

 protected:
  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();

    auto touch_to_fill_delegate =
        std::make_unique<TouchToFillDelegateAndroidImpl>(&autofill_manager());
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    base::WeakPtr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_weak =
        touch_to_fill_delegate->GetWeakPtr();
    autofill_manager().set_touch_to_fill_delegate(
        std::move(touch_to_fill_delegate));

    // Default setup for successful `TryToShowTouchToFill`.
    ON_CALL(autofill_manager(), CanShowAutofillUi).WillByDefault(Return(true));
    ON_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
        .WillByDefault(Return(true));
    ON_CALL(payments_autofill_client(), ShowTouchToFillIban)
        .WillByDefault(Return(true));
    ON_CALL(payments_autofill_client(), ShowTouchToFillLoyaltyCard)
        .WillByDefault(Return(true));
    // Calling HideTouchToFillPaymentMethod in production code leads to that
    // OnDismissed gets triggered (HideTouchToFillPaymentMethod calls
    // view->Hide() on java side, which in its turn triggers onDismissed). Here
    // we mock this call.
    ON_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod)
        .WillByDefault([delegate = touch_to_fill_delegate_weak] {
          if (delegate) {
            delegate->OnDismissed(/*dismissed_by_user=*/false,
                                  /*should_reshow=*/false);
          }
        });
    autofill::MockFastCheckoutClient* fast_checkout_client =
        static_cast<autofill::MockFastCheckoutClient*>(
            autofill_client().GetFastCheckoutClient());
    ON_CALL(*fast_checkout_client, IsNotShownYet)
        .WillByDefault(testing::Return(true));
  }

  // Helper method to add the given `card` and create a card form.
  void ConfigureForCreditCards(const CreditCard& card) {
    form_ = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
    test_api(form_).field(0).set_is_focusable(true);
    autofill_client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .AddCreditCard(card);
  }

  // Helper method to add an IBAN and create an IBAN form.
  std::string ConfigureForIbans() {
    Iban iban;
    iban.set_value(std::u16string(test::kIbanValue16));
    std::string guid = autofill_client()
                           .GetPersonalDataManager()
                           .test_payments_data_manager()
                           .AddAsLocalIban(std::move(iban));
    form_ = test::CreateTestIbanFormData(/*value=*/"");
    test_api(form_).field(0).set_is_focusable(true);
    return guid;
  }

  void ConfigureForLoyaltyCards() {
    LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
    // The touch-to-fill bottom sheet is shown only if the user has at least
    // 1 saved loyalty card.
    test_api(*autofill_client().GetValuablesDataManager())
        .AddLoyaltyCard(loyalty_card);
    form_ = test::CreateTestLoyaltyCardFormData();
    test_api(form_).field(0).set_is_focusable(true);
    // The current URL matches the loyalty card merchant domain.
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL("https://domain.example"));
  }

  void OnFormsSeen() {
    if (!autofill_manager().FindCachedFormById(form_.global_id())) {
      autofill_manager().OnFormsSeen({form_}, {});
    }
  }

  void IntendsToShowTouchToFill(bool expected_success) {
    OnFormsSeen();
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IntendsToShowTouchToFill(
                  form_.global_id(), form_.fields()[0].global_id()));
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client(),
                HideAutofillSuggestions(
                    SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);

    OnFormsSeen();
    EXPECT_EQ(expected_success, touch_to_fill_delegate_->TryToShowTouchToFill(
                                    form_, form_.fields()[0]));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  FormData form_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList features_;
  raw_ptr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       BnplSuggestionSelected_WithValidAmount) {
  std::optional<int64_t> extracted_amount = 12345;
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnDidAcceptBnplSuggestion(extracted_amount, _));

  touch_to_fill_delegate_->BnplSuggestionSelected(extracted_amount);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       BnplSuggestionSelected_WithNullAmount) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnDidAcceptBnplSuggestion(testing::Eq(std::nullopt), _));

  touch_to_fill_delegate_->BnplSuggestionSelected(
      /*extracted_amount=*/std::nullopt);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       BnplSuggestionSelected_CallbackFillsForm) {
  CreditCard test_card = test::GetCreditCard();

  ConfigureForCreditCards(test_card);
  TryToShowTouchToFill(/*expected_success=*/true);

  base::OnceCallback<void(const CreditCard&)> captured_callback;
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnDidAcceptBnplSuggestion(_, _))
      .WillOnce([&](std::optional<uint64_t> amount,
                    base::OnceCallback<void(const CreditCard&)> callback) {
        captured_callback = std::move(callback);
      });

  touch_to_fill_delegate_->BnplSuggestionSelected(
      /*extracted_amount=*/12345);
  ASSERT_TRUE(captured_callback);

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(
          mojom::ActionPersistence::kFill, form_, form_.fields()[0].global_id(),
          ::testing::VariantWith<const CreditCard*>(Pointee(test_card)),
          AutofillTriggerSource::kTouchToFillCreditCard));

  // Run the captured callback, simulating a successful VCN fetch.
  std::move(captured_callback).Run(test_card);
}

TEST_F(TouchToFillDelegateAndroidImplUnitTest,
       BnplSuggestionSelected_CallbackDoesNothingAfterDelegateReset) {
  CreditCard test_card = test::GetCreditCard();

  ConfigureForCreditCards(test_card);
  TryToShowTouchToFill(/*expected_success=*/true);

  base::OnceCallback<void(const CreditCard&)> captured_callback;
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnDidAcceptBnplSuggestion(_, _))
      .WillOnce([&](std::optional<uint64_t> amount,
                    base::OnceCallback<void(const CreditCard&)> callback) {
        captured_callback = std::move(callback);
      });

  touch_to_fill_delegate_->BnplSuggestionSelected(
      /*extracted_amount=*/12345);
  ASSERT_TRUE(captured_callback);

  // Expect FillOrPreviewForm is not called after delegate is reset.
  autofill_manager().set_touch_to_fill_delegate(nullptr);
  EXPECT_CALL(autofill_manager(), FillOrPreviewForm).Times(0);

  std::move(captured_callback).Run(test_card);
}

// Params of TouchToFillDelegateAndroidImplPaymentMethodUnitTest:
// -- FillingProduct: Indicates the Autofill data type to test. Supported data
// types are:
// * Credit card
// * IBAN
// * Loyalty card
class TouchToFillDelegateAndroidImplPaymentMethodUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest,
      public testing::WithParamInterface<FillingProduct> {
 protected:
  void SetUp() override {
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    switch (GetFillingProduct()) {
      case FillingProduct::kCreditCard:
        ConfigureForCreditCards(test::GetCreditCard());
        break;
      case FillingProduct::kIban:
        ConfigureForIbans();
        break;
      case FillingProduct::kLoyaltyCard:
        ConfigureForLoyaltyCards();
        break;
      default:
        NOTREACHED() << "Unsupported filling product: "
                     << FillingProductToString(GetFillingProduct());
    }
  }

  FillingProduct GetFillingProduct() const { return GetParam(); }

  std::string GetTriggerOutcomeHistogramName() {
    switch (GetFillingProduct()) {
      case FillingProduct::kCreditCard:
        return kUmaTouchToFillCreditCardTriggerOutcome;
      case FillingProduct::kIban:
        return kUmaTouchToFillIbanTriggerOutcome;
      case FillingProduct::kLoyaltyCard:
        return kUmaTouchToFillLoyaltyCardTriggerOutcome;
      default:
        NOTREACHED() << "Unsupported filling product: "
                     << FillingProductToString(GetFillingProduct());
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
                         testing::ValuesIn({FillingProduct::kCreditCard,
                                            FillingProduct::kIban,
                                            FillingProduct::kLoyaltyCard}));

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForInvalidForm) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  test_api(autofill_manager()).ClearFormStructures();

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

  EXPECT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());

  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/false,
                                       /*should_reshow=*/false);

  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       OnDismissedRunsCancelCallbackWhenDismissedByUser) {
  TryToShowTouchToFill(/*expected_success=*/true);
  base::MockCallback<base::OnceClosure> mock_cancel_callback;
  touch_to_fill_delegate_->SetCancelCallback(mock_cancel_callback.Get());

  EXPECT_CALL(mock_cancel_callback, Run());
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       OnDismissedDoesNotRunCancelCallbackWhenDismissedBySystem) {
  TryToShowTouchToFill(/*expected_success=*/true);
  base::MockCallback<base::OnceClosure> mock_cancel_callback;
  touch_to_fill_delegate_->SetCancelCallback(mock_cancel_callback.Get());

  EXPECT_CALL(mock_cancel_callback, Run()).Times(0);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/false,
                                       /*should_reshow=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownBeforeAndShouldNotReshow_FlagOff) {
  TryToShowTouchToFill(/*expected_success=*/true);

  ASSERT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());

  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/false);

  EXPECT_CALL(autofill_client(),
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  TryToShowTouchToFill(/*expected_success=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownBeforeAndShouldReshow_FlagOff) {
  TryToShowTouchToFill(/*expected_success=*/true);

  ASSERT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());

  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/true);

  TryToShowTouchToFill(/*expected_success=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillShownIfShownBeforeAndShouldReshow_FlagOn) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(
      features::kAutofillEnableTouchToFillReshowForBnpl);

  TryToShowTouchToFill(/*expected_success=*/true);

  ASSERT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());

  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/true);

  TryToShowTouchToFill(/*expected_success=*/true);

  ASSERT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownBeforeAndShouldNotReshow_FlagOn) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(
      features::kAutofillEnableTouchToFillReshowForBnpl);

  TryToShowTouchToFill(/*expected_success=*/true);

  ASSERT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());

  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/false);

  TryToShowTouchToFill(/*expected_success=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownCurrently) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client(),
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(
      touch_to_fill_delegate_->TryToShowTouchToFill(form_, form_.fields()[0]));
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(autofill_manager(), DidShowSuggestions);
  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfWasShownAndShouldNotBeShownAgain) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(GetTriggerOutcomeHistogramName(),
                                      TouchToFillPaymentMethodTriggerOutcome::
                                          kShownBeforeAndShouldNotBeShownAgain,
                                      1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfFieldIsNotFocusable) {
  test_api(form_).field(0).set_is_focusable(false);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  test_api(form_).field(0).set_value(u"Initial value");

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfNoDataOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearAllLocalData();
  test_api(*autofill_client().GetValuablesDataManager()).ClearLoyaltyCards();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_manager(), CanShowAutofillUi)
      .WillRepeatedly(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kCannotShowAutofillUi, 1);
}

class TouchToFillDelegateAndroidImplCreditCardUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 public:
  static std::vector<CreditCard> GetCardsToSuggest(
      std::vector<const CreditCard*> credit_cards) {
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
  autofill_manager().OnFormsSeen({form_}, {});
  // Set credit card value.
  // TODO(crbug.com/40900766): Retrieve the card number field by name here.
  ASSERT_EQ(form_.fields()[1].name(), u"cardnumber");
  test_api(form_).field(1).set_value(u"411111111111");
  // Force a cache update so it knows about the field edit.
  autofill_manager().OnFormsSeen({form_}, {});
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
  autofill_manager().OnFormsSeen({form_}, {});
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
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("http://example.test"));

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
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_no_number = test::GetCreditCard();
  cc_no_number.SetNumber(u"");
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(cc_no_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_exp_date = test::GetCreditCard();
  cc_no_exp_date.SetExpirationMonth(0);
  cc_no_exp_date.SetExpirationYear(0);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(cc_no_exp_date);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_name = test::GetCreditCard();
  cc_no_name.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(cc_no_name);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 3);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfTheOnlyCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
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
          autofill_client().GetFastCheckoutClient());
  EXPECT_CALL(*fast_checkout_client, IsNotShownYet).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFastCheckoutWasShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillSucceedsIfAtLestOneCardIsValid) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillCreditCard)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsExpiredCards) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(expired_card);
  std::vector<const CreditCard*> credit_cards = GetCreditCardsToSuggest(
      autofill_client().GetPersonalDataManager().payments_data_manager());

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _,
          ElementsAre(
              EqualsSuggestionFields(
                  credit_cards[0]->CardNameForAutofillDisplay(
                      credit_cards[0]->nickname()),
                  credit_cards[0]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*has_deactivated_style=*/false),
              EqualsSuggestionFields(
                  credit_cards[1]->CardNameForAutofillDisplay(
                      credit_cards[1]->nickname()),
                  credit_cards[1]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*has_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillDoesNotShowDisusedExpiredCards) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard disused_expired_card = test::GetExpiredCreditCard();
  credit_card.usage_history().set_use_date(AutofillClock::Now());
  disused_expired_card.usage_history().set_use_date(
      AutofillClock::Now() - kDisusedDataModelTimeDelta * 2);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(disused_expired_card);
  ASSERT_TRUE(credit_card.IsCompleteValidCard());
  ASSERT_FALSE(disused_expired_card.IsCompleteValidCard());
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAre(EqualsSuggestionFields(
                 credit_card.CardNameForAutofillDisplay(credit_card.nickname()),
                 credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                 /*has_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsVirtualCardSuggestionsForEnrolledCards) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  CreditCard virtual_card = CreditCard::CreateVirtualCard(credit_card);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(false));

  // Since the card is enrolled into the virtual cards feature, a virtual card
  // suggestion should be created and added before the real card.
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAre(
                 EqualsSuggestionFields(
                     virtual_card.CardNameForAutofillDisplay(
                         virtual_card.nickname()),
                     virtual_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                     /*has_deactivated_style=*/false),
                 EqualsSuggestionFields(
                     credit_card.CardNameForAutofillDisplay(
                         credit_card.nickname()),
                     credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                     /*has_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       SafelyHideTouchToFillInDtor) {
  payments_autofill_client()
      .ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards();
  TryToShowTouchToFill(/*expected_success=*/true);

  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
}

// Add one IBAN to the PDM and verify that IBAN is not shown for the credit
// card form.
TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       PassTheCreditCardsToTheClient) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddAsLocalIban(std::move(iban1));
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card1);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card2);
  std::vector<const CreditCard*> credit_cards = GetCreditCardsToSuggest(
      autofill_client().GetPersonalDataManager().payments_data_manager());

  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _,
          ElementsAre(
              EqualsSuggestionFields(
                  credit_cards[0]->CardNameForAutofillDisplay(
                      credit_cards[0]->nickname()),
                  credit_cards[0]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*has_deactivated_style=*/false),
              EqualsSuggestionFields(
                  credit_cards[1]->CardNameForAutofillDisplay(
                      credit_cards[1]->nickname()),
                  credit_cards[1]->ObfuscatedNumberWithVisibleLastFourDigits(),
                  /*has_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);

  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ScanCreditCardIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);
  EXPECT_CALL(payments_autofill_client(), ScanCreditCard);
  touch_to_fill_delegate_->ScanCreditCard();

  CreditCard credit_card = autofill::test::GetCreditCard();
  EXPECT_CALL(autofill_manager(), FillOrPreviewForm);
  touch_to_fill_delegate_->OnCreditCardScanned(credit_card);
  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ShowCreditCardSettingsIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSettings(testing::Eq(SuggestionType::kManageCreditCard)));
  touch_to_fill_delegate_->ShowPaymentMethodSettings();

  ASSERT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionClosesTheSheet) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionFillsCardForm) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_manager(), FillOrPreviewForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       VirtualCardSelectionFillsCardForm) {
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_manager(), FillOrPreviewForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       AutofillUsedAfterTouchToFillDismissal) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true,
                                       /*should_reshow=*/false);

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
       OnBnplIssuerSuggestionSelected) {
  TryToShowTouchToFill(/*expected_success=*/true);

  BnplIssuer issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm);
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddBnplIssuer(issuer);

  base::MockCallback<base::OnceCallback<void(BnplIssuer)>>
      mock_selected_issuer_callback;
  touch_to_fill_delegate_->SetSelectedIssuerCallback(
      mock_selected_issuer_callback.Get());

  EXPECT_CALL(mock_selected_issuer_callback, Run(issuer)).Times(1);

  touch_to_fill_delegate_->OnBnplIssuerSuggestionSelected(
      /*issuer_id=*/"affirm");
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       OnBnplIssuerSuggestionSelected_NoCallbackSet) {
  TryToShowTouchToFill(/*expected_success=*/true);

  BnplIssuer issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm);
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddBnplIssuer(issuer);

  base::MockCallback<base::OnceCallback<void(BnplIssuer)>>
      mock_selected_issuer_callback;

  EXPECT_CALL(mock_selected_issuer_callback, Run(issuer)).Times(0);

  touch_to_fill_delegate_->OnBnplIssuerSuggestionSelected(
      /*issuer_id=*/"affirm");
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       OnBnplIssuerSuggestionSelected_NoMatchingIssuer) {
  TryToShowTouchToFill(/*expected_success=*/true);

  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddBnplIssuer(issuer);

  base::MockCallback<base::OnceCallback<void(BnplIssuer)>>
      mock_selected_issuer_callback;
  touch_to_fill_delegate_->SetSelectedIssuerCallback(
      mock_selected_issuer_callback.Get());

  EXPECT_CALL(mock_selected_issuer_callback, Run(issuer)).Times(0);

  touch_to_fill_delegate_->OnBnplIssuerSuggestionSelected(
      /*issuer_id=*/"invalidIssuerId");
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest, OnBnplTosAccepted) {
  TryToShowTouchToFill(/*expected_success=*/true);

  base::MockCallback<base::OnceClosure> mock_accept_tos_callback;
  touch_to_fill_delegate_->SetBnplTosAcceptCallback(
      mock_accept_tos_callback.Get());

  EXPECT_CALL(mock_accept_tos_callback, Run);

  touch_to_fill_delegate_->OnBnplTosAccepted();
}

class TouchToFillDelegateAndroidImplIbanUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 protected:
  void SetUp() override {
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
      autofill_client().GetPersonalDataManager().test_payments_data_manager();
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

  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillIban)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       SafelyHideTouchToFillInDtor) {
  payments_autofill_client()
      .ExpectDelegateWeakPtrFromShowInvalidatedOnHideForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
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
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .SetSyncingForTest(true);
  std::string guid = ConfigureForIbans();
  // Add a server IBAN with a different instrument_id and verify `FetchValue`
  // is not triggered.
  long instrument_id = 123245678L;
  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(instrument_id));
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddServerIban(server_iban);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*payments_autofill_client().GetIbanAccessManager(), FetchValue);
  touch_to_fill_delegate_->IbanSuggestionSelected(
      Iban::InstrumentId(instrument_id));
}

class TouchToFillDelegateAndroidImplLoyaltyCardUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 protected:
  void SetUp() override {
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForLoyaltyCards();
  }
};

TEST_F(TouchToFillDelegateAndroidImplLoyaltyCardUnitTest,
       TryToShowTouchToFillFailsIfShowLoyaltyCardsFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillLoyaltyCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplLoyaltyCardUnitTest,
       PassTheLoyaltyCardsToTheClient) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  LoyaltyCard card2 = test::CreateLoyaltyCard2();
  std::vector<LoyaltyCard> loyalty_cards{card2, card1};
  // Makes sure there is at least one affiliated card available.
  autofill_client().set_last_committed_primary_main_frame_url(
      card1.merchant_domains()[0]);
  test_api(*autofill_client().GetValuablesDataManager())
      .SetLoyaltyCards(loyalty_cards);

  // Cards must be sorted by merchant name.
  EXPECT_CALL(payments_autofill_client(),
              ShowTouchToFillLoyaltyCard(_, ElementsAre(card1, card2)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplLoyaltyCardUnitTest,
       TryToShowTouchToFillFailsIfNoMatchingDomains) {
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://non-matching.domain"));
  std::vector<LoyaltyCard> loyalty_cards{test::CreateLoyaltyCard()};
  test_api(*autofill_client().GetValuablesDataManager())
      .SetLoyaltyCards(loyalty_cards);

  EXPECT_CALL(payments_autofill_client(), ShowTouchToFillLoyaltyCard).Times(0);

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplLoyaltyCardUnitTest,
       SafelyHideTouchToFillInDtor) {
  payments_autofill_client()
      .ExpectDelegateWeakPtrFromShowInvalidatedOnHideForLoyaltyCards();
  TryToShowTouchToFill(/*expected_success=*/true);

  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
}

TEST_F(TouchToFillDelegateAndroidImplLoyaltyCardUnitTest,
       LoyaltyCardSelectionFillsFormAndHidesSheet) {
  const LoyaltyCard kLoyaltyCard = test::CreateLoyaltyCard();
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(payments_autofill_client(), HideTouchToFillPaymentMethod);
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          _, _, base::UTF8ToUTF16(kLoyaltyCard.loyalty_card_number()),
          SuggestionType::kLoyaltyCardEntry, Optional(LOYALTY_MEMBERSHIP_ID)));
  EXPECT_CALL(autofill_manager(),
              LogAndRecordLoyaltyCardFill(kLoyaltyCard, _, _));
  touch_to_fill_delegate_->LoyaltyCardSuggestionSelected(kLoyaltyCard);
}

class TouchToFillDelegateAndroidImplEmailOrLoyaltyCardUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 protected:
  void SetUp() override {
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForEmailOrLoyaltyCards();
  }

  void ConfigureForEmailOrLoyaltyCards() {
    LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
    // The touch-to-fill bottom sheet is shown only if the user has at least
    // 1 saved loyalty card.
    test_api(*autofill_client().GetValuablesDataManager())
        .AddLoyaltyCard(loyalty_card);
    form_ = test::CreateTestEmailOrLoyaltyCardFormData();
    test_api(form_).field(0).set_is_focusable(true);
    // The current URL matches the loyalty card merchant domain.
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL("https://domain.example"));
  }
};

// Make sure the TTF bottom sheet if offered on EMAIL_OR_LOYALTY_MEMBERSHIP_ID
// fields.
TEST_F(TouchToFillDelegateAndroidImplEmailOrLoyaltyCardUnitTest,
       PassTheLoyaltyCardsToTheClient) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  LoyaltyCard card2 = test::CreateLoyaltyCard2();
  std::vector<LoyaltyCard> loyalty_cards{card2, card1};
  // Makes sure there is at least one affiliated card available.
  autofill_client().set_last_committed_primary_main_frame_url(
      card1.merchant_domains()[0]);
  test_api(*autofill_client().GetValuablesDataManager())
      .SetLoyaltyCards(loyalty_cards);

  // Cards must be sorted by merchant name.
  EXPECT_CALL(payments_autofill_client(),
              ShowTouchToFillLoyaltyCard(_, ElementsAre(card1, card2)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

class TouchToFillDelegateAndroidImplVcnGrayOutForMerchantOptOutUnitTest
    : public TouchToFillDelegateAndroidImplCreditCardUnitTest {
 public:
  void SetUp() override {
    TouchToFillDelegateAndroidImplCreditCardUnitTest::SetUp();
    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(true));
  }
};

TEST_F(TouchToFillDelegateAndroidImplVcnGrayOutForMerchantOptOutUnitTest,
       TryToShowTouchToFillWithVirtualCardSuggestionsForOptedOutMerchants) {
  CreditCard credit_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  credit_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(credit_card);
  CreditCard virtual_card = CreditCard::CreateVirtualCard(credit_card);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  // Since VCN gray-out feature is active, the `HasDeactivatedStyle()`
  // should return true for the virtual card suggestion. However,
  // `HasDeactivatedStyle()` should return false for the associated real credit
  // card suggestion.
  EXPECT_CALL(
      payments_autofill_client(),
      ShowTouchToFillCreditCard(
          _, ElementsAre(
                 EqualsSuggestionFields(
                     virtual_card.CardNameForAutofillDisplay(
                         virtual_card.nickname()),
                     virtual_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                     /*has_deactivated_style=*/true),
                 EqualsSuggestionFields(
                     credit_card.CardNameForAutofillDisplay(
                         credit_card.nickname()),
                     credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                     /*has_deactivated_style=*/false))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

}  // namespace
}  // namespace autofill
