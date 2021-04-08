// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_message_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::_;

class SaveAddressProfileMessageControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveAddressProfileMessageControllerTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveAddressProfileMessageController::PrimaryActionCallback
          action_callback);
  void ExpectDismissMessageCall();

  void TriggerActionClick();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();
  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  AutofillProfile profile_ = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      save_callback_;
  base::MockCallback<SaveAddressProfileMessageController::PrimaryActionCallback>
      action_callback_;

 private:
  SaveAddressProfileMessageController controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  base::test::ScopedFeatureList feature_list_;
};

void SaveAddressProfileMessageControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  feature_list_.InitAndEnableFeature(
      features::kAutofillAddressProfileSavePrompt);
}

void SaveAddressProfileMessageControllerTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void SaveAddressProfileMessageControllerTest::EnqueueMessage(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback save_callback,
    SaveAddressProfileMessageController::PrimaryActionCallback
        action_callback) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  controller_.DisplayMessage(web_contents(), profile, std::move(save_callback),
                             std::move(action_callback));
}

void SaveAddressProfileMessageControllerTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   content::WebContents* web_contents,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveAddressProfileMessageControllerTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SaveAddressProfileMessageControllerTest::TriggerMessageDismissedCallback(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
}

messages::MessageWrapper*
SaveAddressProfileMessageControllerTest::GetMessageWrapper() {
  return controller_.message_.get();
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button.
TEST_F(SaveAddressProfileMessageControllerTest, ProceedOnActionClick) {
  EnqueueMessage(profile_, save_callback_.Get(), action_callback_.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(action_callback_, Run(_, profile_, _));
  TriggerActionClick();
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(save_callback_, Run(_, profile_)).Times(0);
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the save callback is triggered with
// |SaveAddressProfileOfferUserDecision::kDeclined| when the user dismisses the
// message.
TEST_F(SaveAddressProfileMessageControllerTest, DeclineOnGestureDismiss) {
  EnqueueMessage(profile_, save_callback_.Get(), action_callback_.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          profile_));
  TriggerMessageDismissedCallback(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the save callback is triggered with
// |SaveAddressProfileOfferUserDecision::kIgnored| when the message is
// autodismissed.
TEST_F(SaveAddressProfileMessageControllerTest, IgnoreOnTimerAutodismiss) {
  EnqueueMessage(profile_, save_callback_.Get(), action_callback_.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(save_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  TriggerMessageDismissedCallback(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveAddressProfileMessageControllerTest, OnlyOnePromptAtATime) {
  EnqueueMessage(profile_, save_callback_.Get(), action_callback_.Get());

  AutofillProfile another_profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  base::MockCallback<SaveAddressProfileMessageController::PrimaryActionCallback>
      another_action_callback;
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  ExpectDismissMessageCall();
  EnqueueMessage(another_profile, another_save_callback.Get(),
                 another_action_callback.Get());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

}  // namespace autofill
