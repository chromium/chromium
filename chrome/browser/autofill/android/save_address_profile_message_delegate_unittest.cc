// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_message_delegate.h"

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

class SaveAddressProfileMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveAddressProfileMessageDelegateTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback callback);
  void ExpectDismissMessageCall();

  void TriggerActionClick();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();
  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

 private:
  SaveAddressProfileMessageDelegate delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

void SaveAddressProfileMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void SaveAddressProfileMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void SaveAddressProfileMessageDelegateTest::EnqueueMessage(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_.DisplaySavePrompt(web_contents(), profile, std::move(callback));
}

void SaveAddressProfileMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   content::WebContents* web_contents,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveAddressProfileMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SaveAddressProfileMessageDelegateTest::TriggerMessageDismissedCallback(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
}

messages::MessageWrapper*
SaveAddressProfileMessageDelegateTest::GetMessageWrapper() {
  return delegate_.message_.get();
}

// Tests that the save prompt is treated as accepted when the user clicks "Save"
// button.
TEST_F(SaveAddressProfileMessageDelegateTest, SaveOnActionClick) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillAddressProfileSavePrompt);

  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));

  EnqueueMessage(profile, callback.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the save prompt is treated as declined when the user dismisses the
// message.
TEST_F(SaveAddressProfileMessageDelegateTest, DeclineOnGestureDismiss) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillAddressProfileSavePrompt);

  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          profile));

  EnqueueMessage(profile, callback.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerMessageDismissedCallback(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the save prompt is treated as ignored when the message is
// autodismissed.
TEST_F(SaveAddressProfileMessageDelegateTest, IgnoreOnTimerAutodismiss) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillAddressProfileSavePrompt);

  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile));

  EnqueueMessage(profile, callback.Get());
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerMessageDismissedCallback(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveAddressProfileMessageDelegateTest, OnlyOnePromptAtATime) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillAddressProfileSavePrompt);

  AutofillProfile profile1 = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      callback1;
  EXPECT_CALL(callback1,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile1));
  EnqueueMessage(profile1, callback1.Get());

  ExpectDismissMessageCall();
  AutofillProfile profile2 = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      callback2;
  EnqueueMessage(profile2, callback2.Get());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

}  // namespace autofill
