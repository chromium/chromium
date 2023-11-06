// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"

#include <memory>

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_controller.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;
using ::testing::Return;

namespace autofill {

namespace {

class MockTouchToFillCreditCardViewImpl : public TouchToFillCreditCardView {
 public:
  MockTouchToFillCreditCardViewImpl() {
    ON_CALL(*this, Show).WillByDefault(Return(true));
  }
  ~MockTouchToFillCreditCardViewImpl() override = default;

  MOCK_METHOD(bool,
              Show,
              (TouchToFillCreditCardViewController * controller,
               base::span<const CreditCard> cards_to_suggest,
               bool should_show_scan_credit_card));
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
  MOCK_METHOD(void, ShowCreditCardSettings, (), (override));
  MOCK_METHOD(void,
              SuggestionSelected,
              (std::string unique_id, bool is_virtual),
              (override));
  MOCK_METHOD(void, OnDismissed, (bool dismissed_by_user), (override));

 private:
  std::unique_ptr<TouchToFillKeyboardSuppressor> suppressor_;
  base::WeakPtrFactory<MockTouchToFillDelegateAndroidImpl> weak_factory_{this};
};

class TestContentAutofillClientWithTouchToFillCreditCardController
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  TouchToFillCreditCardController& credit_card_controller() {
    return credit_card_controller_;
  }

 private:
  TouchToFillCreditCardController credit_card_controller_{this};
};

}  // namespace

class TouchToFillCreditCardControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillCreditCardControllerTest() = default;
  ~TouchToFillCreditCardControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));
    autofill_manager().set_touch_to_fill_delegate(
        std::make_unique<MockTouchToFillDelegateAndroidImpl>(
            &autofill_manager()));
    mock_view_ = std::make_unique<MockTouchToFillCreditCardViewImpl>();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestContentAutofillClientWithTouchToFillCreditCardController&
  autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  TouchToFillCreditCardController& credit_card_controller() {
    return autofill_client().credit_card_controller();
  }

  MockTouchToFillDelegateAndroidImpl& ttf_delegate() {
    return *static_cast<MockTouchToFillDelegateAndroidImpl*>(
        autofill_manager().touch_to_fill_delegate());
  }

  const std::vector<const CreditCard> credit_cards_ = {test::GetCreditCard(),
                                                       test::GetCreditCard2()};
  std::unique_ptr<MockTouchToFillCreditCardViewImpl> mock_view_;

  void OnBeforeAskForValuesToFill() {
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(false));
    EXPECT_CALL(ttf_delegate(), IntendsToShowTouchToFill)
        .WillOnce(Return(true));
    credit_card_controller()
        .keyboard_suppressor_for_test()
        .OnBeforeAskForValuesToFill(autofill_manager(), some_form_, some_field_,
                                    some_form_data_);
    EXPECT_TRUE(credit_card_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

  void OnAfterAskForValuesToFill() {
    EXPECT_TRUE(credit_card_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
    EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill).WillOnce(Return(true));
    credit_card_controller()
        .keyboard_suppressor_for_test()
        .OnAfterAskForValuesToFill(autofill_manager(), some_form_, some_field_);
    EXPECT_TRUE(credit_card_controller()
                    .keyboard_suppressor_for_test()
                    .is_suppressing());
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<
      TestContentAutofillClientWithTouchToFillCreditCardController>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  FormData some_form_data_ =
      autofill::test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                   /*use_month_type=*/false);
  FormGlobalId some_form_ = some_form_data_.global_id();
  FieldGlobalId some_field_ = test::MakeFieldGlobalId();
};

TEST_F(TouchToFillCreditCardControllerTest, ShowPassesCardsToTheView) {
  // Test that the cards have propagated to the view.
  EXPECT_CALL(*mock_view_, Show(&credit_card_controller(),
                                ElementsAreArray(credit_cards_), true));
  OnBeforeAskForValuesToFill();
  credit_card_controller().Show(std::move(mock_view_),
                                ttf_delegate().GetWeakPointer(), credit_cards_);
  OnAfterAskForValuesToFill();
}

TEST_F(TouchToFillCreditCardControllerTest, ScanCreditCardIsCalled) {
  OnBeforeAskForValuesToFill();
  credit_card_controller().Show(std::move(mock_view_),
                                ttf_delegate().GetWeakPointer(), credit_cards_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ScanCreditCard);
  credit_card_controller().ScanCreditCard(nullptr);
}

TEST_F(TouchToFillCreditCardControllerTest, ShowCreditCardSettingsIsCalled) {
  OnBeforeAskForValuesToFill();
  credit_card_controller().Show(std::move(mock_view_),
                                ttf_delegate().GetWeakPointer(), credit_cards_);
  OnAfterAskForValuesToFill();
  EXPECT_CALL(ttf_delegate(), ShowCreditCardSettings);
  credit_card_controller().ShowCreditCardSettings(nullptr);
}

TEST_F(TouchToFillCreditCardControllerTest, OnDismissedIsCalled) {
  OnBeforeAskForValuesToFill();
  credit_card_controller().Show(std::move(mock_view_),
                                ttf_delegate().GetWeakPointer(), credit_cards_);
  OnAfterAskForValuesToFill();

  EXPECT_CALL(ttf_delegate(), OnDismissed);
  credit_card_controller().OnDismissed(nullptr, true);
}

}  // namespace autofill
