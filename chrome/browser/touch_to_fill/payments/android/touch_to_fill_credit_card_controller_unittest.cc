// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include <memory>

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ::autofill::CreditCard;
using ::testing::ElementsAreArray;
using ::testing::Return;

class MockTouchToFillCreditCardViewImpl
    : public autofill::TouchToFillCreditCardView {
 public:
  MockTouchToFillCreditCardViewImpl() {
    ON_CALL(*this, Show).WillByDefault(Return(true));
  }
  ~MockTouchToFillCreditCardViewImpl() override = default;

  MOCK_METHOD(bool,
              Show,
              (autofill::TouchToFillCreditCardViewController * controller,
               base::span<const autofill::CreditCard> cards_to_suggest,
               bool should_show_scan_credit_card));
  MOCK_METHOD(void, Hide, ());
};

class MockAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  using autofill::TestBrowserAutofillManager::TestBrowserAutofillManager;
  MockAutofillManager(const MockAutofillManager&) = delete;
  MockAutofillManager& operator=(const MockAutofillManager&) = delete;
  ~MockAutofillManager() override = default;

  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), (override));
};

class MockTouchToFillDelegateImpl : public autofill::TouchToFillDelegate {
 public:
  MockTouchToFillDelegateImpl() {
    ON_CALL(*this, GetManager).WillByDefault(Return(&manager_));
    ON_CALL(*this, ShouldShowScanCreditCard).WillByDefault(Return(true));
  }
  ~MockTouchToFillDelegateImpl() override = default;

  base::WeakPtr<MockTouchToFillDelegateImpl> GetWeakPointer() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(void,
              TryToShowTouchToFill,
              (int query_id,
               const autofill::FormData& form,
               const autofill::FormFieldData& field));
  MOCK_METHOD(autofill::AutofillManager*, GetManager, (), (override));
  MOCK_METHOD(bool, HideTouchToFill, (), ());
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
  autofill::TestAutofillClient client_;
  autofill::TestAutofillDriver driver_;
  MockAutofillManager manager_{&driver_, &client_};
  base::WeakPtrFactory<MockTouchToFillDelegateImpl> weak_factory_{this};
};

}  // namespace

class TouchToFillCreditCardControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillCreditCardControllerTest() = default;
  ~TouchToFillCreditCardControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_view_ = std::make_unique<MockTouchToFillCreditCardViewImpl>();
  }

  std::unique_ptr<MockTouchToFillCreditCardViewImpl> mock_view_;
  MockTouchToFillDelegateImpl mock_delegate_;
  const std::vector<const autofill::CreditCard> credit_cards_ = {
      autofill::test::GetCreditCard(), autofill::test::GetCreditCard2()};
  // The object to be tested.
  autofill::TouchToFillCreditCardController credit_card_controller_;
};

TEST_F(TouchToFillCreditCardControllerTest, ShowPassesCardsToTheView) {
  // Test that the cards have ptopagated to the view.
  EXPECT_CALL(*mock_view_,
              Show(&credit_card_controller_, ElementsAreArray(credit_cards_),
                   testing::Eq(true)));

  credit_card_controller_.Show(std::move(mock_view_),
                               mock_delegate_.GetWeakPointer(), credit_cards_);
}

TEST_F(TouchToFillCreditCardControllerTest, ScanCreditCardIsCalled) {
  credit_card_controller_.Show(std::move(mock_view_),
                               mock_delegate_.GetWeakPointer(), credit_cards_);
  EXPECT_CALL(mock_delegate_, ScanCreditCard);
  credit_card_controller_.ScanCreditCard(nullptr);
}

TEST_F(TouchToFillCreditCardControllerTest, ShowCreditCardSettingsIsCalled) {
  credit_card_controller_.Show(std::move(mock_view_),
                               mock_delegate_.GetWeakPointer(), credit_cards_);
  EXPECT_CALL(mock_delegate_, ShowCreditCardSettings);
  credit_card_controller_.ShowCreditCardSettings(nullptr);
}

TEST_F(TouchToFillCreditCardControllerTest, OnDismissedIsCalled) {
  credit_card_controller_.Show(std::move(mock_view_),
                               mock_delegate_.GetWeakPointer(), credit_cards_);

  EXPECT_CALL(mock_delegate_, OnDismissed);
  credit_card_controller_.OnDismissed(nullptr, true);
}
