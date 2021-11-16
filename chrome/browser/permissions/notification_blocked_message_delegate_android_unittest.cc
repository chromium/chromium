// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_blocked_message_delegate_android.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"

class MockDelegate : public NotificationBlockedMessageDelegate::Delegate {
 public:
  ~MockDelegate() override = default;
  MockDelegate(const base::WeakPtr<permissions::PermissionPromptAndroid>&
                   permission_prompt) {}

  MOCK_METHOD(void, Accept, (), (override));
  MOCK_METHOD(void, Deny, (), (override));

  MOCK_METHOD(void, Closing, (), (override));
  MOCK_METHOD(bool, IsPromptDestroyed, (), (override));

  MOCK_METHOD(bool, ShouldUseQuietUI, (), (override));
};

class NotificationBlockedMessageDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  NotificationBlockedMessageDelegateAndroidTest() = default;
  ~NotificationBlockedMessageDelegateAndroidTest() override = default;

  void ExpectEnqueued() {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  }

  void ShowMessage(std::unique_ptr<MockDelegate> delegate) {
    controller_ = std::make_unique<NotificationBlockedMessageDelegate>(
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

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_->message_.get();
  }

  std::unique_ptr<MockDelegate> GetMockDelegate() {
    return std::move(delegate_);
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<NotificationBlockedMessageDelegate> controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockDelegate> delegate_;
};

void NotificationBlockedMessageDelegateAndroidTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  delegate_ = std::make_unique<MockDelegate>(nullptr);
}

void NotificationBlockedMessageDelegateAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(NotificationBlockedMessageDelegateAndroidTest, DismissByTimeout) {
  auto delegate = GetMockDelegate();
  EXPECT_CALL(*delegate, IsPromptDestroyed)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*delegate, Closing);
  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerDismiss(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(NotificationBlockedMessageDelegateAndroidTest, DismissByPrimaryAction) {
  auto delegate = GetMockDelegate();
  EXPECT_CALL(*delegate, IsPromptDestroyed)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*delegate, Closing);
  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny);

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerPrimaryAction();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(NotificationBlockedMessageDelegateAndroidTest,
       DismissByPrimaryActionWhenPromptDestroyed) {
  auto delegate = GetMockDelegate();
  EXPECT_CALL(*delegate, IsPromptDestroyed)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*delegate, Closing).Times(0);
  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);

  ExpectEnqueued();
  ShowMessage(std::move(delegate));
  TriggerPrimaryAction();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}
