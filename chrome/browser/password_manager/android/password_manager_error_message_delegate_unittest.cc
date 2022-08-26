// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class PasswordManagerErrorMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordManagerErrorMessageDelegateTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  void DisplayMessageAndExpecteEnqueued(bool save_password);

  void DismissMessageAndExpectDismissed(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

 private:
  std::unique_ptr<PasswordManagerErrorMessageDelegate> delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

PasswordManagerErrorMessageDelegateTest::
    PasswordManagerErrorMessageDelegateTest()
    : delegate_(std::make_unique<PasswordManagerErrorMessageDelegate>()) {}

void PasswordManagerErrorMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void PasswordManagerErrorMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void PasswordManagerErrorMessageDelegateTest::DisplayMessageAndExpecteEnqueued(
    bool save_password) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->DisplayPasswordManagerErrorMessage(web_contents(), save_password);
}

void PasswordManagerErrorMessageDelegateTest::DismissMessageAndExpectDismissed(
    messages::DismissReason dismiss_reason) {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate_->DismissPasswordManagerErrorMessage(dismiss_reason);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

messages::MessageWrapper*
PasswordManagerErrorMessageDelegateTest::GetMessageWrapper() {
  return delegate_->message_.get();
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for "sign in to save password" message.
TEST_F(PasswordManagerErrorMessageDelegateTest,
       MessagePropertyValuesSignInToSavePassword) {
  DisplayMessageAndExpecteEnqueued(/*save_password=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SIGN_IN_TO_SAVE_PASSWORDS),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessageAndExpectDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for "sign in to use password" message.
TEST_F(PasswordManagerErrorMessageDelegateTest,
       MessagePropertyValuesSignInToUsePassword) {
  DisplayMessageAndExpecteEnqueued(/*save_password=*/false);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SIGN_IN_TO_USE_PASSWORDS),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessageAndExpectDismissed(messages::DismissReason::UNKNOWN);
}
