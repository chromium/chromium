// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/public/fast_checkout/proto/actions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Helper function for creating a wait_for_user_selection action.
autofill_assistant::external::Action CreateWaitForUserSelectionAction() {
  autofill_assistant::external::Action action;
  autofill_assistant::fast_checkout::FastCheckoutAction fast_checkout_action;
  autofill_assistant::fast_checkout::WaitForFastCheckoutUserSelection
      wait_for_user_selection;
  *fast_checkout_action.mutable_wait_for_user_selection() =
      wait_for_user_selection;
  *action.mutable_info()->mutable_fast_checkout_action() = fast_checkout_action;
  return action;
}

class FastCheckoutExternalActionDelegateTest : public testing::Test {
 public:
  FastCheckoutExternalActionDelegateTest() = default;

  FastCheckoutExternalActionDelegate* delegate() {
    return &external_action_delegate_;
  }

 private:
  FastCheckoutExternalActionDelegate external_action_delegate_;
};

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnActionRequested_EmptyAction_IsNotSuccessful) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(autofill_assistant::external::Action(),
                                /* is_interrupt= */ false,
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());

  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(FastCheckoutExternalActionDelegateTest,
       OnActionRequested_SelectionBeforeRequest_ResultIsSuccessful) {
  auto autofill_profile = std::make_unique<autofill::AutofillProfile>();
  autofill_profile->SetInfo(kServerFieldType, kName, kLocale);
  auto credit_card = std::make_unique<autofill::CreditCard>();
  credit_card->set_instrument_id(kInstrumentId);
  delegate()->SetOptionsSelected(*autofill_profile, *credit_card);

  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(CreateWaitForUserSelectionAction(),
                                /* is_interrupt= */ false,
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());

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
       SetOptionsSelected_SelectionAfterRequest_ResultIsSuccessful) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      end_action_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(end_action_callback, Run).WillOnce(testing::SaveArg<0>(&result));

  delegate()->OnActionRequested(CreateWaitForUserSelectionAction(),
                                /* is_interrupt= */ false,
                                start_dom_checks_callback.Get(),
                                end_action_callback.Get());
  // Here `result` must not have been set yet. It will be after the
  // `SetOptionsSelected` call.
  EXPECT_FALSE(result.has_success());
  EXPECT_FALSE(result.has_selected_credit_card());
  EXPECT_EQ(result.selected_profiles_size(), 0);

  auto autofill_profile = std::make_unique<autofill::AutofillProfile>();
  autofill_profile->SetInfo(kServerFieldType, kName, kLocale);
  auto credit_card = std::make_unique<autofill::CreditCard>();
  credit_card->set_instrument_id(kInstrumentId);
  delegate()->SetOptionsSelected(*autofill_profile, *credit_card);

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
