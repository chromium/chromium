// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/fast_checkout/proto/fast_checkout.pb.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::ServerFieldType;

namespace {
constexpr ServerFieldType kServerFieldType = ServerFieldType::NAME_FULL;
constexpr char kLocale[] = "en-US";
constexpr char16_t kName[] = u"Jane Doe";
constexpr int kInstrumentId = 91077;
constexpr char kProfileName[] = "SHIPPING";
}  // namespace

using DomUpdateCallback =
    autofill_assistant::ExternalActionDelegate::DomUpdateCallback;

// Helper function for creating a "show bottomsheet" action.
autofill_assistant::external::Action CreateShowBottomsheetAction() {
  autofill_assistant::external::Action action;
  FastCheckoutAction fast_checkout_action;
  ShowFastCheckoutBottomSheet show_bottom_sheet;
  fast_checkout_action.mutable_show_bottom_sheet()->CopyFrom(show_bottom_sheet);
  fast_checkout_action.SerializeToString(
      action.mutable_info()->mutable_action_payload());
  return action;
}

class MockFastCheckoutController : public FastCheckoutController {
 public:
  MockFastCheckoutController() : FastCheckoutController() {}
  ~MockFastCheckoutController() override = default;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void,
              OnOptionsSelected,
              (std::unique_ptr<autofill::AutofillProfile> profile,
               std::unique_ptr<autofill::CreditCard> credit_card),
              (override));
  MOCK_METHOD(void, OnDismiss, (), (override));
};

class FastCheckoutExternalActionDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutExternalActionDelegateTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto mock_controller = std::make_unique<MockFastCheckoutController>();
    mock_controller_ = mock_controller.get();
    external_action_delegate_ =
        std::make_unique<FastCheckoutExternalActionDelegate>(web_contents());
    external_action_delegate_->SetFastCheckoutControllerForTest(
        std::move(mock_controller));
  }

  FastCheckoutExternalActionDelegate* delegate() {
    return external_action_delegate_.get();
  }

  MockFastCheckoutController* controller() { return mock_controller_.get(); }

 private:
  std::unique_ptr<FastCheckoutExternalActionDelegate> external_action_delegate_;
  raw_ptr<MockFastCheckoutController> mock_controller_;
};

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnActionRequested_ShowBottomsheetAction_ShowsBottomsheet) {
  EXPECT_CALL(*controller(), Show);

  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  delegate()->OnActionRequested(CreateShowBottomsheetAction(),
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());
}

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnActionRequested_EmptyAction_DoesNotShowBottomsheetAndIsNotSuccessful) {
  EXPECT_CALL(*controller(), Show).Times(0);

  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(autofill_assistant::external::Action(),
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());

  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnOptionsSelected_ValidSelections_ResultIsSuccessful) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(CreateShowBottomsheetAction(),
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());
  auto autofill_profile = std::make_unique<autofill::AutofillProfile>();
  autofill_profile->SetInfo(kServerFieldType, kName, kLocale);
  auto credit_card = std::make_unique<autofill::CreditCard>();
  credit_card->set_instrument_id(kInstrumentId);
  delegate()->OnOptionsSelected(std::move(autofill_profile),
                                std::move(credit_card));

  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.has_selected_credit_card());
  EXPECT_EQ(result.selected_credit_card().instrument_id(), kInstrumentId);
  EXPECT_GT(result.selected_profiles_size(), 0);
  EXPECT_EQ(
      result.selected_profiles().at(kProfileName).values().at(kServerFieldType),
      base::UTF16ToUTF8(kName));
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnOptionsSelected_InvalidSelections_ResultIsNotSuccessful) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(CreateShowBottomsheetAction(),
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());
  delegate()->OnOptionsSelected(nullptr,
                                std::make_unique<autofill::CreditCard>());

  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnDismiss_ResultIsNotSuccessful) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(CreateShowBottomsheetAction(),
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());
  delegate()->OnDismiss();

  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
  EXPECT_FALSE(result.has_result_info());
}
