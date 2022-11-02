// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/android/mock_password_manager_error_message_helper_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }

  PasswordManagerErrorMessageDelegate* delegate() { return delegate_.get(); }

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  base::MockOnceClosure* mock_dismissal_callback() {
    return &mock_dismissal_callback_;
  }

  messages::MessageWrapper* GetMessageWrapper();

 private:
  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<PasswordManagerErrorMessageDelegate> delegate_;
  base::MockOnceClosure mock_dismissal_callback_;
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
  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kTimesUPMAuthErrorShown, 0);
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
  delegate_->MaybeDisplayErrorMessage(web_contents(), pref_service(), flow_type,
                                      error_type,
                                      mock_dismissal_callback_.Get());
}

void PasswordManagerErrorMessageDelegateTest::DismissMessageAndExpectDismissed(
    messages::DismissReason dismiss_reason) {
  EXPECT_CALL(mock_dismissal_callback_, Run);
  // In production code this method is called as a result of a java action.
  // Since that is not possible in a unit test, the method is invoked directly.
  delegate_->HandleMessageDismissed(dismiss_reason);
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
  // Trigger the click action on the "Sign in" button and dismiss the message.
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // The message needs to be dismissed manually in tests. In production code
  // this happens automatically, but on the java side.
  DismissMessageAndExpectDismissed(messages::DismissReason::PRIMARY_ACTION);
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
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage).Times(0);
  EXPECT_CALL(*mock_dismissal_callback(), Run);
  delegate()->MaybeDisplayErrorMessage(
      web_contents(), pref_service(),
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable,
      mock_dismissal_callback()->Get());
}

TEST_F(PasswordManagerErrorMessageDelegateTest, DisplaySavesTimestamp) {
  EXPECT_CALL(*helper_bridge(), SaveErrorUIShownTimestamp());
  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
}

TEST_F(PasswordManagerErrorMessageDelegateTest, DisplayIncreasesCounter) {
  ASSERT_EQ(0, pref_service()->GetInteger(
                   password_manager::prefs::kTimesUPMAuthErrorShown));
  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
  EXPECT_EQ(1, pref_service()->GetInteger(
                   password_manager::prefs::kTimesUPMAuthErrorShown));
}

TEST_F(PasswordManagerErrorMessageDelegateTest,
       CounterDoesntIncreaseWhenShouldntShow) {
  ASSERT_EQ(0, pref_service()->GetInteger(
                   password_manager::prefs::kTimesUPMAuthErrorShown));
  EXPECT_CALL(*helper_bridge(), ShouldShowErrorUI()).WillOnce(Return(false));
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage).Times(0);
  delegate()->MaybeDisplayErrorMessage(
      web_contents(), pref_service(),
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable,
      base::DoNothing());
  EXPECT_EQ(0, pref_service()->GetInteger(
                   password_manager::prefs::kTimesUPMAuthErrorShown));
}

TEST_F(PasswordManagerErrorMessageDelegateTest,
       DismissErrorMessageDequeuesMessageIfItExists) {
  DisplayMessageAndExpectEnqueued(
      password_manager::ErrorMessageFlowType::kSaveFlow,
      password_manager::PasswordStoreBackendErrorType::kAuthErrorResolvable);
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(*message_dispatcher_bridge(),
              DismissMessage(testing::Eq(GetMessageWrapper()),
                             messages::DismissReason::UNKNOWN));
  delegate()->DismissPasswordManagerErrorMessage(
      messages::DismissReason::UNKNOWN);
}
