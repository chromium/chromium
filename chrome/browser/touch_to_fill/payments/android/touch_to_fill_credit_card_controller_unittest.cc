// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include <memory>

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ::autofill::CreditCard;
using ::testing::ElementsAreArray;

class MockTouchToFillCreditCardViewImpl
    : public autofill::TouchToFillCreditCardView {
 public:
  MockTouchToFillCreditCardViewImpl() = default;
  ~MockTouchToFillCreditCardViewImpl() override = default;

  MOCK_METHOD(bool,
              Show,
              (autofill::TouchToFillCreditCardViewController * controller,
               base::span<const autofill::CreditCard* const> cards_to_suggest));
  MOCK_METHOD(void, Hide, ());
};

// virtual AutofillDriver* GetDriver()

class MockTouchToFillDelegateImpl : public autofill::TouchToFillDelegate {
 public:
  MockTouchToFillDelegateImpl() = default;
  ~MockTouchToFillDelegateImpl() override = default;

  base::WeakPtr<MockTouchToFillDelegateImpl> GetWeakPointer() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(void,
              TryToShowTouchToFill,
              (int query_id,
               const autofill::FormData& form,
               const autofill::FormFieldData& field));
  MOCK_METHOD(autofill::AutofillDriver*, GetDriver, (), (override));

 private:
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
  // The object to be tested.
  autofill::TouchToFillCreditCardController credit_card_controller_;
};

TEST_F(TouchToFillCreditCardControllerTest, ShowPassesCardsToTheView) {
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  std::vector<autofill::CreditCard*> credit_cards = {&credit_card1,
                                                     &credit_card2};

  // Test that the cards have ptopagated to the view.
  EXPECT_CALL(*mock_view_,
              Show(&credit_card_controller_, ElementsAreArray(credit_cards)));

  credit_card_controller_.Show(std::move(mock_view_),
                               mock_delegate_.GetWeakPointer(), credit_cards);
}
