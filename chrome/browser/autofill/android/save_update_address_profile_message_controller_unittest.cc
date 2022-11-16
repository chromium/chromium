// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_message_controller.h"

#include "base/android/jni_android.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using testing::_;

class SaveUpdateAddressProfileMessageControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveUpdateAddressProfileMessageControllerTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueSaveMessage(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback) {
    EnqueueMessage(profile, nullptr, std::move(save_callback),
                   std::move(action_callback));
  }
  void EnqueueUpdateMessage(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback) {
    EnqueueMessage(profile, original_profile, std::move(save_callback),
                   std::move(action_callback));
  }
  void ExpectDismissMessageCall();

  void TriggerActionClick();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();

  AutofillProfile profile_;
  AutofillProfile original_profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      save_callback_;
  base::MockCallback<
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback>
      action_callback_;

 private:
  void EnqueueMessage(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback);

  SaveUpdateAddressProfileMessageController controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  base::test::ScopedFeatureList feature_list_;
};

void SaveUpdateAddressProfileMessageControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  profile_ = test::GetFullProfile();
  original_profile_ = test::GetFullProfile2();
}

void SaveUpdateAddressProfileMessageControllerTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void SaveUpdateAddressProfileMessageControllerTest::EnqueueMessage(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback save_callback,
    SaveUpdateAddressProfileMessageController::PrimaryActionCallback
        action_callback) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  controller_.DisplayMessage(web_contents(), profile, original_profile,
                             std::move(save_callback),
                             std::move(action_callback));
  EXPECT_TRUE(controller_.IsMessageDisplayed());
}

void SaveUpdateAddressProfileMessageControllerTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveUpdateAddressProfileMessageControllerTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_TRUE(controller_.IsMessageDisplayed());
}

void SaveUpdateAddressProfileMessageControllerTest::
    TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  EXPECT_FALSE(controller_.IsMessageDisplayed());
}

messages::MessageWrapper*
SaveUpdateAddressProfileMessageControllerTest::GetMessageWrapper() {
  return controller_.message_.get();
}

// Tests that the save message properties (title, description with profile
// details, primary button text, icon) are set correctly.
TEST_F(SaveUpdateAddressProfileMessageControllerTest, SaveMessageContent) {
  EnqueueSaveMessage(profile_, save_callback_.Get(), action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(1, GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the update message properties (title, description with original
// profile details, primary button text, icon) are set correctly.
TEST_F(SaveUpdateAddressProfileMessageControllerTest, UpdateMessageContent) {
  EnqueueUpdateMessage(profile_, &original_profile_, save_callback_.Get(),
                       action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(u"Jane A. Smith, 123 Main Street",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(1, GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the save message.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       ProceedOnActionClickWhenSave) {
  EnqueueSaveMessage(profile_, save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(action_callback_, Run(_, profile_, nullptr, _));
  TriggerActionClick();

  EXPECT_CALL(save_callback_, Run(_, profile_)).Times(0);
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the update message.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       ProceedOnActionClickWhenUpdate) {
  EnqueueUpdateMessage(profile_, &original_profile_, save_callback_.Get(),
                       action_callback_.Get());

  EXPECT_CALL(action_callback_, Run(_, profile_, &original_profile_, _));
  TriggerActionClick();

  EXPECT_CALL(save_callback_, Run(_, profile_)).Times(0);
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the save callback is triggered with
// |SaveAddressProfileOfferUserDecision::kMessageDeclined| when the user
// dismisses the message via gesture.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       DecisionIsMessageDeclinedOnGestureDismiss) {
  EnqueueSaveMessage(profile_, save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kMessageDeclined,
          profile_));
  TriggerMessageDismissedCallback(messages::DismissReason::GESTURE);
}

// Tests that the save callback is triggered with
// |SaveAddressProfileOfferUserDecision::kMessageTimeout| when the message is
// auto-dismissed after a timeout.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       DecisionIsMessageTimeoutOnTimerAutodismiss) {
  EnqueueSaveMessage(profile_, save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kMessageTimeout,
          profile_));
  TriggerMessageDismissedCallback(messages::DismissReason::TIMER);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveUpdateAddressProfileMessageControllerTest, OnlyOnePromptAtATime) {
  EnqueueUpdateMessage(profile_, &original_profile_, save_callback_.Get(),
                       action_callback_.Get());

  AutofillProfile another_profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  base::MockCallback<
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback>
      another_action_callback;
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  ExpectDismissMessageCall();
  EnqueueSaveMessage(another_profile, another_save_callback.Get(),
                     another_action_callback.Get());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

}  // namespace autofill
