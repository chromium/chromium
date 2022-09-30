// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/android/mock_password_manager_error_message_helper_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::Return;

namespace {
constexpr char kErrorMessageDismissalReasonHistogramName[] =
    "PasswordManager.ErrorMessageDismissalReason";
constexpr char kErrorMessageDisplayReasonHistogramName[] =
    "PasswordManager.ErrorMessageDisplayReason";
}

class PasswordManagerErrorMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordManagerErrorMessageDelegateTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  void DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType flow_type,
      password_manager::PasswordStoreBackendErrorType error_type);

  void ExpectDismissed(messages::DismissReason dismiss_reason);

  void DismissMessageAndExpectDismissed(messages::DismissReason dismiss_reason);

  MockPasswordManagerErrorMessageHelperBridge* helper_bridge() {
    return helper_bridge_;
  }

  PasswordManagerErrorMessageDelegate* delegate() { return delegate_.get(); }

  const messages::MockMessageDispatcherBridge& message_dispatcher_bridge() {
    return message_dispatcher_bridge_;
  }

  messages::MessageWrapper* GetMessageWrapper();

 private:
  std::unique_ptr<PasswordManagerErrorMessageDelegate> delegate_;
  // The `helper_bridge_` is owned by the `delegate_`.
  raw_ptr<MockPasswordManagerErrorMessageHelperBridge> helper_bridge_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

PasswordManagerErrorMessageDelegateTest::
    PasswordManagerErrorMessageDelegateTest() {
  auto mock_helper_bridge =
      std::make_unique<MockPasswordManagerErrorMessageHelperBridge>();
  helper_bridge_ = mock_helper_bridge.get();
  delegate_ = std::make_unique<PasswordManagerErrorMessageDelegate>(
      std::move(mock_helper_bridge));
}

void PasswordManagerErrorMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void PasswordManagerErrorMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void PasswordManagerErrorMessageDelegateTest::DisplayMessageAndExpectEnqueued(
    password_manager::ErrorMessageFlowType flow_type,
    password_manager::PasswordStoreBackendErrorType error_type) {
  EXPECT_CALL(*helper_bridge_, ShouldShowErrorUI()).WillOnce(Return(true));
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->MaybeDisplayErrorMessage(web_contents(), flow_type, error_type,
                                      base::DoNothing());
}

void PasswordManagerErrorMessageDelegateTest::ExpectDismissed(
    messages::DismissReason dismiss_reason) {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void PasswordManagerErrorMessageDelegateTest::DismissMessageAndExpectDismissed(
    messages::DismissReason dismiss_reason) {
  ExpectDismissed(dismiss_reason);
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
  base::HistogramTester histogram_tester;

  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SIGN_IN_TO_SAVE_PASSWORDS),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessageAndExpectDismissed(messages::DismissReason::UNKNOWN);

  histogram_tester.ExpectUniqueSample(
      kErrorMessageDisplayReasonHistogramName,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable, 1);
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for "sign in to use password" message.
TEST_F(PasswordManagerErrorMessageDelegateTest,
       MessagePropertyValuesSignInToUsePassword) {
  base::HistogramTester histogram_tester;

  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kFillFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorUnresolvable);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SIGN_IN_TO_USE_PASSWORDS),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessageAndExpectDismissed(messages::DismissReason::UNKNOWN);

  histogram_tester.ExpectUniqueSample(
      kErrorMessageDisplayReasonHistogramName,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorUnresolvable,
      1);
}

// Tests that the sign in flow starts when the user clicks the "Sign in" button
// and that the metrics are recorded correctly.
TEST_F(PasswordManagerErrorMessageDelegateTest, SignInOnActionClick) {
  base::HistogramTester histogram_tester;

  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(*helper_bridge(),
              StartUpdateAccountCredentialsFlow(web_contents()));
  ExpectDismissed(messages::DismissReason::PRIMARY_ACTION);
  // Trigger the click action on the "Sign in" button and dismiss the message.
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());

  histogram_tester.ExpectUniqueSample(kErrorMessageDismissalReasonHistogramName,
                                      messages::DismissReason::PRIMARY_ACTION,
                                      1);
}

// Tests that the metrics are recorded correctly when the message is
// autodismissed.
TEST_F(PasswordManagerErrorMessageDelegateTest, MetricOnAutodismissTimer) {
  base::HistogramTester histogram_tester;

  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
  EXPECT_NE(nullptr, GetMessageWrapper());

  DismissMessageAndExpectDismissed(messages::DismissReason::TIMER);

  histogram_tester.ExpectUniqueSample(kErrorMessageDismissalReasonHistogramName,
                                      messages::DismissReason::TIMER, 1);
}

TEST_F(PasswordManagerErrorMessageDelegateTest,
       NotDisplayedWhenCondiditonNotMet) {
  EXPECT_CALL(*helper_bridge(), ShouldShowErrorUI()).WillOnce(Return(false));
  EXPECT_CALL(message_dispatcher_bridge(), EnqueueMessage).Times(0);
  delegate()->MaybeDisplayErrorMessage(
      web_contents(), password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable,
      base::DoNothing());
}

TEST_F(PasswordManagerErrorMessageDelegateTest, DisplayeSavesTimestamp) {
  EXPECT_CALL(*helper_bridge(), SaveErrorUIShownTimestamp());
  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
}
