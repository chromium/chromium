// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"

#include <memory>
#include <vector>

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;

class MockTouchToFillPaymentMethodViewImpl : public TouchToFillPaymentMethodView {
 public:
  MockTouchToFillPaymentMethodViewImpl() {
    ON_CALL(*this, ShowCreditCards).WillByDefault(Return(true));
    ON_CALL(*this, ShowIbans).WillByDefault(Return(true));
  }
  ~MockTouchToFillPaymentMethodViewImpl() override = default;

  MOCK_METHOD(bool,
              ShowCreditCards,
              ((TouchToFillPaymentMethodViewController * controller),
               (base::span<const Suggestion> suggestions),
               (bool should_show_scan_credit_card)));
  MOCK_METHOD(bool,
              ShowIbans,
              (TouchToFillPaymentMethodViewController * controller,
               base::span<const Iban> ibans_to_suggest));
  MOCK_METHOD(bool,
              ShowLoyaltyCards,
              (TouchToFillPaymentMethodViewController * controller,
               base::span<const LoyaltyCard> loyalty_cards_to_suggest));
  MOCK_METHOD(void, Hide, ());
};

class MockTouchToFillDelegateAndroidImpl
    : public TouchToFillDelegateAndroidImpl {
 public:
  explicit MockTouchToFillDelegateAndroidImpl(
      TestBrowserAutofillManager* autofill_manager)
      : TouchToFillDelegateAndroidImpl(autofill_manager) {
    ON_CALL(*this, GetManager).WillByDefault(Return(autofill_manager));
    ON_CALL(*this, ShouldShowScanCreditCard).WillByDefault(Return(true));
  }
  ~MockTouchToFillDelegateAndroidImpl() override = default;

  base::WeakPtr<MockTouchToFillDelegateAndroidImpl> GetWeakPointer() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(bool,
              IntendsToShowTouchToFill,
              (FormGlobalId, FieldGlobalId, const FormData&),
              (override));
  MOCK_METHOD(TestBrowserAutofillManager*, GetManager, (), (override));
  MOCK_METHOD(bool, ShouldShowScanCreditCard, (), (override));
  MOCK_METHOD(void, ScanCreditCard, (), (override));
  MOCK_METHOD(void, OnCreditCardScanned, (const CreditCard& card), (override));
  MOCK_METHOD(void, ShowPaymentMethodSettings, (), (override));
  MOCK_METHOD(void,
              CreditCardSuggestionSelected,
              (std::string unique_id, bool is_virtual),
              (override));
  MOCK_METHOD(void, OnDismissed, (bool dismissed_by_user), (override));

 private:
  std::unique_ptr<TouchToFillKeyboardSuppressor> suppressor_;
  base::WeakPtrFactory<MockTouchToFillDelegateAndroidImpl> weak_factory_{this};
};

class TestContentAutofillClientWithTouchToFillPaymentMethodController
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  TouchToFillPaymentMethodController& payment_method_controller() {
    return payment_method_controller_;
  }

 private:
  TouchToFillPaymentMethodController payment_method_controller_{this};
};

class TouchToFillPaymentMethodControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillPaymentMethodControllerTest() = default;
  ~TouchToFillPaymentMethodControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<MockTouchToFillDelegateAndroidImpl>(
            &autofill_manager()));
    mock_view_ = std::make_unique<MockTouchToFillPaymentMethodViewImpl>();
  }

  void SetUpIbanFormField() {
    some_form_data_ = autofill::test::CreateTestIbanFormData();
    some_form_ = some_form_data_.global_id();
    some_field_ = test::MakeFieldGlobalId();
  }

  void SetUpLoyaltyCardFormField() {
    some_form_data_ = test::CreateTestLoyaltyCardFormData();
    some_form_ = some_form_data_.global_id();
    some_field_ = test::MakeFieldGlobalId();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestContentAutofillClientWithTouchToFillPaymentMethodController&
  autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  TouchToFillPaymentMethodController& payment_method_controller() {
    return autofill_client().payment_method_controller();
  }

  MockTouchToFillDelegateAndroidImpl& ttf_delegate() {
    return *static_cast<MockTouchToFillDelegateAndroidImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

  const std::vector<CreditCard> credit_cards_ = {test::GetCreditCard(),
                                                 test::GetCreditCard2()};
  const std::vector<Iban> ibans_ = {test::GetLocalIban(),
                                    test::GetServerIban()};
  const std::vector<LoyaltyCard> loyalty_cards_ = {test::CreateLoyaltyCard(),
                                                   test::CreateLoyaltyCard2()};
  const std::vector<Suggestion> suggestions_{
      test::CreateAutofillSuggestion(
          credit_cards_[0].CardNameForAutofillDisplay(),
          credit_cards_[0].ObfuscatedNumberWithVisibleLastFourDigits(),
          /*has_deactivated_style=*/false),
      test::CreateAutofillSuggestion(
          credit_cards_[1].CardNameForAutofillDisplay(),
          credit_cards_[1].ObfuscatedNumberWithVisibleLastFourDigits(),
          /*has_deactivated_style=*/false)};
  std::unique_ptr<MockTouchToFillPaymentMethodViewImpl> mock_view_;

  void OnBeforeAskForValuesToFill() {
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(false));
    EXPECT_CALL(ttf_delegate(), IntendsToShowTouchToFill)
        .WillOnce(Return(true));
    payment_method_controller()
        .keyboard_suppressor_for_test()
        .OnBeforeAskForValuesToFill(autofill_manager(), some_form_, some_field_,
                                    some_form_data_);
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

  void OnAfterAskForValuesToFill() {
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(true));
    payment_method_controller()
        .keyboard_suppressor_for_test()
        .OnAfterAskForValuesToFill(autofill_manager(), some_form_, some_field_);
    EXPECT_TRUE(payment_method_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<
      TestContentAutofillClientWithTouchToFillPaymentMethodController>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  FormData some_form_data_ =
      autofill::test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                   /*use_month_type=*/false);
  FormGlobalId some_form_ = some_form_data_.global_id();
  FieldGlobalId some_field_ = test::MakeFieldGlobalId();
};

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowCreditCardsPassesCardsToTheView) {
  // Test that the cards have propagated to the view.
  EXPECT_CALL(*mock_view_,
              ShowCreditCards(&payment_method_controller(),
                              ElementsAreArray(suggestions_),
                              /*should_show_scan_credit_card=*/true));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowCreditCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, ShowIbansPassesIbansToTheView) {
  SetUpIbanFormField();
  // Test that the IBANs have propagated to the view.
  EXPECT_CALL(*mock_view_, ShowIbans(&payment_method_controller(),
                                     ElementsAreArray(ibans_)));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowIbans(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), ibans_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowLoyaltyCardsPassesLoyaltyCardsToTheView) {
  SetUpLoyaltyCardFormField();
  // Test that the loyalty cards have propagated to the view.
  EXPECT_CALL(*mock_view_, ShowLoyaltyCards(&payment_method_controller(),
                                            ElementsAreArray(loyalty_cards_)));
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowLoyaltyCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), loyalty_cards_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillPaymentMethodControllerTest, ScanCreditCardIsCalled) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowCreditCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ScanCreditCard);
  payment_method_controller().ScanCreditCard(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowPaymentMethodSettingsIsCalledForCards) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowCreditCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ShowPaymentMethodSettings);
  payment_method_controller().ShowPaymentMethodSettings(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest,
       ShowPaymentMethodSettingsIsCalledForIbans) {
  SetUpIbanFormField();
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowIbans(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), ibans_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ShowPaymentMethodSettings);
  payment_method_controller().ShowPaymentMethodSettings(nullptr);
}

TEST_F(TouchToFillPaymentMethodControllerTest, OnDismissedIsCalled) {
  OnBeforeAskForValuesToFill();
  payment_method_controller().ShowCreditCards(
      std::move(mock_view_), ttf_delegate().GetWeakPointer(), suggestions_);
  OnAfterAskForValuesToFill();

  EXPECT_CALL(ttf_delegate(), OnDismissed);
  payment_method_controller().OnDismissed(nullptr, true);
}

}  // namespace
}  // namespace autofill
