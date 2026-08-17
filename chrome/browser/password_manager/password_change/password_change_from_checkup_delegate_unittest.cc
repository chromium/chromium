// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using State = PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState;
using ActuatorState = PasswordChangeActuator::State;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

class MockPasswordChangeActuator : public PasswordChangeActuator {
 public:
  MockPasswordChangeActuator() = default;
  ~MockPasswordChangeActuator() override = default;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(content::WebContents*,
              GetExecutorWebContents,
              (),
              (const, override));
  MOCK_METHOD(void, OpenPasswordChangeTab, (content::WebContents*), (override));
  MOCK_METHOD(std::u16string, GetGeneratedPassword, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));

  base::WeakPtr<MockPasswordChangeActuator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeActuator> weak_ptr_factory_{this};
};

password_manager::StoredCredential CreateTestCredential() {
  GURL url("https://example.com/password");
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = url::Origin::Create(url).GetURL().spec();
  form.username_value = u"test_user@example.com";
  form.password_value = u"test_password";
  return password_manager::FromPasswordForm(std::move(form));
}

class PasswordChangeFromCheckupDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangeFromCheckupDelegateTest() = default;
  ~PasswordChangeFromCheckupDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    delegate_ = std::make_unique<PasswordChangeFromCheckupDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::WeakPtr<MockPasswordChangeActuator> InjectMockActuator() {
    auto mock_actuator =
        std::make_unique<NiceMock<MockPasswordChangeActuator>>();
    base::WeakPtr<MockPasswordChangeActuator> actuator_ptr =
        mock_actuator->GetWeakPtr();
    delegate_->set_actuator_for_testing(std::move(mock_actuator));
    return actuator_ptr;
  }

  PasswordChangeFromCheckupDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<PasswordChangeFromCheckupDelegate> delegate_;
};

TEST_F(PasswordChangeFromCheckupDelegateTest, StartFlowStartsActuator) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, Start());
  EXPECT_CALL(*mock_actuator, Cancel());
  EXPECT_CALL(*mock_actuator, RemoveObserver(delegate()));

  base::MockRepeatingCallback<void(State)> callback;
  EXPECT_CALL(callback, Run(State::kInactive));

  delegate()->StartPasswordChangeFlow(
      CreateTestCredential(), web_contents()->GetWeakPtr(), callback.Get());

  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       StartFlowWithNullWebContentsDoesNotStartActuator) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, Start()).Times(0);

  base::MockRepeatingCallback<void(State)> callback;
  EXPECT_CALL(callback, Run).Times(0);

  delegate()->StartPasswordChangeFlow(CreateTestCredential(), nullptr,
                                      callback.Get());

  EXPECT_TRUE(delegate()->generated_password().empty());
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       OnActuationStateChangedTransitionsCorrectly) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, Start());
  EXPECT_CALL(*mock_actuator, Cancel());
  EXPECT_CALL(*mock_actuator, RemoveObserver(delegate()));

  base::MockRepeatingCallback<void(State)> callback;
  delegate()->StartPasswordChangeFlow(
      CreateTestCredential(), web_contents()->GetWeakPtr(), callback.Get());

  const struct {
    ActuatorState actuator_state;
    State expected_delegate_state;
  } kTestCases[] = {
      {ActuatorState::kWaitingForChangePasswordForm, State::kAttemptingSignIn},
      {ActuatorState::kChangingPassword, State::kChangingPassword},
      {ActuatorState::kPasswordSuccessfullyChanged,
       State::kPasswordChangedSuccessfully},
      {ActuatorState::kPasswordChangeFailed, State::kError},
      {ActuatorState::kChangePasswordFormNotFound, State::kError},
      {ActuatorState::kOtpDetected, State::kError},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(callback, Run(test_case.expected_delegate_state));
    delegate()->OnActuationStateChanged(test_case.actuator_state);
  }

  EXPECT_CALL(callback, Run(State::kInactive));
  delegate()->Stop(actor::ActorTask::StoppedReason::kShutdown);
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       StopCancelsActuatorEmitsInactiveAndResetsCallback) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, Start());
  EXPECT_CALL(*mock_actuator, Cancel());
  EXPECT_CALL(*mock_actuator, RemoveObserver(delegate()));

  base::MockRepeatingCallback<void(State)> callback;
  EXPECT_CALL(callback, Run(State::kInactive));

  delegate()->StartPasswordChangeFlow(
      CreateTestCredential(), web_contents()->GetWeakPtr(), callback.Get());

  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);

  // Subsequent state changes should not trigger the callback because
  // Stop() resets it.
  EXPECT_CALL(callback, Run).Times(0);
  delegate()->OnActuationStateChanged(
      ActuatorState::kPasswordSuccessfullyChanged);

  // Generated password should be empty after actuator is reset by Stop().
  EXPECT_TRUE(delegate()->generated_password().empty());
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       StopWithoutActuatorOrCallbackDoesNotCrash) {
  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       OnActuationStateChangedWithoutCallbackDoesNotCrash) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, Start());
  EXPECT_CALL(*mock_actuator, Cancel());
  EXPECT_CALL(*mock_actuator, RemoveObserver(delegate()));

  // Start flow without providing a callback.
  delegate()->StartPasswordChangeFlow(CreateTestCredential(),
                                      web_contents()->GetWeakPtr());

  // Should safely no-op without callback registered.
  delegate()->OnActuationStateChanged(
      ActuatorState::kWaitingForChangePasswordForm);
  delegate()->OnActuationStateChanged(ActuatorState::kChangingPassword);
  delegate()->OnActuationStateChanged(
      ActuatorState::kPasswordSuccessfullyChanged);
  delegate()->OnActuationStateChanged(ActuatorState::kPasswordChangeFailed);
  delegate()->OnActuationStateChanged(
      ActuatorState::kChangePasswordFormNotFound);
  delegate()->OnActuationStateChanged(ActuatorState::kOtpDetected);

  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       GeneratedPasswordDelegatesToActuator) {
  // When actuator is not set.
  EXPECT_TRUE(delegate()->generated_password().empty());

  // When actuator is set.
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, GetGeneratedPassword())
      .WillOnce(Return(u"GeneratedP@ssword123"));

  EXPECT_EQ(delegate()->generated_password(), u"GeneratedP@ssword123");
  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);
}

TEST_F(PasswordChangeFromCheckupDelegateTest,
       DestructorCancelsActuatorAndRemovesObserver) {
  auto local_delegate = std::make_unique<PasswordChangeFromCheckupDelegate>();
  auto mock_actuator = std::make_unique<NiceMock<MockPasswordChangeActuator>>();
  EXPECT_CALL(*mock_actuator, Start());
  EXPECT_CALL(*mock_actuator, Cancel());
  EXPECT_CALL(*mock_actuator, RemoveObserver(local_delegate.get()));

  local_delegate->set_actuator_for_testing(std::move(mock_actuator));
  local_delegate->StartPasswordChangeFlow(CreateTestCredential(),
                                          web_contents()->GetWeakPtr());

  local_delegate.reset();
}

TEST_F(PasswordChangeFromCheckupDelegateTest, OpenActuationTabCallsActuator) {
  base::WeakPtr<MockPasswordChangeActuator> mock_actuator =
      InjectMockActuator();
  EXPECT_CALL(*mock_actuator, OpenPasswordChangeTab(web_contents()));

  delegate()->StartPasswordChangeFlow(CreateTestCredential(),
                                      web_contents()->GetWeakPtr());
  delegate()->OpenActuationTab();
  delegate()->Stop(actor::ActorTask::StoppedReason::kStoppedByUser);
}

}  // namespace
