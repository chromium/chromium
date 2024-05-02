// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blocked_message_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"

namespace {

using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;

}  // namespace

class MockDelegate : public PermissionBlockedMessageDelegate::Delegate {
 public:
  ~MockDelegate() override = default;
  MockDelegate(const base::WeakPtr<permissions::PermissionPromptAndroid>&
                   permission_prompt) {}

  MOCK_METHOD(void, Accept, (), (override));
  MOCK_METHOD(void, Deny, (), (override));

  MOCK_METHOD(void, Closing, (), (override));

  MOCK_METHOD(bool, ShouldUseQuietUI, (), (override));
  MOCK_METHOD(std::optional<QuietUiReason>,
              ReasonForUsingQuietUi,
              (),
              (override));
  MOCK_METHOD(ContentSettingsType, GetContentSettingsType, (), (override));
};

class PermissionBlockedMessageDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PermissionBlockedMessageDelegateAndroidTest() = default;
  ~PermissionBlockedMessageDelegateAndroidTest() override = default;

  void ExpectEnqueued() {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  }

  void ExpectDismiss() {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([](messages::MessageWrapper* message,
                     messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(dismiss_reason));
        });
  }

  void ShowMessage(std::unique_ptr<MockDelegate> delegate) {
    controller_ = std::make_unique<PermissionBlockedMessageDelegate>(
        web_contents(), std::move(delegate));
  }

  void TriggerDismiss(messages::DismissReason reason) {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([&reason](messages::MessageWrapper* message,
                            messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(reason));
        });
    controller_->DismissInternal();
    EXPECT_EQ(nullptr, controller_->message_.get());
  }

  void TriggerPrimaryAction() {
    controller_->HandlePrimaryActionClick();
    TriggerDismiss(messages::DismissReason::PRIMARY_ACTION);
  }

  void TriggerManageClick() { controller_->HandleManageClick(); }

  void TriggerDialogOnAllowForThisSite() {
    controller_->OnAllowForThisSite();
    controller_->OnDialogDismissed();
  }

  void TriggerDialogDismiss() { controller_->OnDialogDismissed(); }

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_->message_.get();
  }

  std::unique_ptr<MockDelegate> GetMockDelegate() {
    return std::move(delegate_);
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<PermissionBlockedMessageDelegate> controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockDelegate> delegate_;
  raw_ptr<permissions::PermissionRequestManager> manager_ = nullptr;
};

void PermissionBlockedMessageDelegateAndroidTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  delegate_ = std::make_unique<MockDelegate>(nullptr);
  permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  manager_ =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
}

void PermissionBlockedMessageDelegateAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByTimeout) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerDismiss(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByPrimaryAction) {
  auto delegate = GetMockDelegate();
  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny);
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerPrimaryAction();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByDialogDismissed) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(
          std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, Closing);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogDismiss();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       DismissByDialogOnAllowForThisSite) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(
          std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept);
  EXPECT_CALL(*delegate, Deny).Times(0);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogOnAllowForThisSite();
}
