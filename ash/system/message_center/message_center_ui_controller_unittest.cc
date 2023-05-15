// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_ui_controller.h"

#include <memory>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using base::ASCIIToUTF16;

namespace ash {
namespace {

class TestNotificationDelegate : public message_center::NotificationDelegate {
 public:
  TestNotificationDelegate() = default;

  TestNotificationDelegate(const TestNotificationDelegate&) = delete;
  TestNotificationDelegate& operator=(const TestNotificationDelegate&) = delete;

 private:
  ~TestNotificationDelegate() override = default;
};

class MockDelegate : public MessageCenterUiDelegate {
 public:
  MockDelegate() {}

  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;

  ~MockDelegate() override {}
  void OnMessageCenterContentsChanged() override {}
  bool ShowPopups() override {
    if (!show_message_center_success_)
      return false;

    EXPECT_FALSE(popups_visible_);
    popups_visible_ = true;
    return true;
  }
  void HidePopups() override {
    EXPECT_TRUE(popups_visible_);
    popups_visible_ = false;
  }
  bool ShowMessageCenter() override {
    EXPECT_FALSE(popups_visible_);
    return show_popups_success_;
  }
  void HideMessageCenter() override { EXPECT_FALSE(popups_visible_); }

  bool popups_visible_ = false;
  bool show_popups_success_ = true;
  bool show_message_center_success_ = true;
};

}  // namespace

class MessageCenterUiControllerTest : public AshTestBase {
 public:
  MessageCenterUiControllerTest() {}

  MessageCenterUiControllerTest(const MessageCenterUiControllerTest&) = delete;
  MessageCenterUiControllerTest& operator=(
      const MessageCenterUiControllerTest&) = delete;

  ~MessageCenterUiControllerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<MockDelegate>();
    message_center_ = message_center::MessageCenter::Get();
    ui_controller_ =
        std::make_unique<MessageCenterUiController>(delegate_.get());
  }

  void TearDown() override {
    ui_controller_.reset();
    delegate_.reset();
    message_center_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  message_center::NotifierId DummyNotifierId() {
    return message_center::NotifierId();
  }

  message_center::Notification* AddNotification(const std::string& id) {
    return AddNotification(id, DummyNotifierId());
  }

  message_center::Notification* AddNotification(
      const std::string& id,
      message_center::NotifierId notifier_id) {
    std::unique_ptr<message_center::Notification> notification(
        new message_center::Notification(
            message_center::NOTIFICATION_TYPE_SIMPLE, id,
            u"Test Web Notification", u"Notification message body.",
            ui::ImageModel(), u"www.test.org", GURL(), notifier_id,
            message_center::RichNotificationData(),
            new TestNotificationDelegate()));
    message_center::Notification* notification_ptr = notification.get();
    message_center_->AddNotification(std::move(notification));
    return notification_ptr;
  }
  std::unique_ptr<MockDelegate> delegate_;
  std::unique_ptr<MessageCenterUiController> ui_controller_;
  raw_ptr<message_center::MessageCenter, ExperimentalAsh> message_center_;
};

TEST_F(MessageCenterUiControllerTest, BasicMessageCenter) {
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  bool shown = ui_controller_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

TEST_F(MessageCenterUiControllerTest, BasicPopup) {
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowPopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  AddNotification("BasicPopup");

  ASSERT_TRUE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->HidePopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

TEST_F(MessageCenterUiControllerTest, MessageCenterClosesPopups) {
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  AddNotification("MessageCenterClosesPopups");

  ASSERT_TRUE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  bool shown = ui_controller_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  // The notification is queued if it's added when message center is visible.
  AddNotification("MessageCenterClosesPopups2");

  ui_controller_->ShowPopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  // There is no queued notification.
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowMessageCenterBubble();
  ui_controller_->HideMessageCenterBubble();
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

TEST_F(MessageCenterUiControllerTest, ShowBubbleFails) {
  // Now the delegate will signal that it was unable to show a bubble.
  delegate_->show_popups_success_ = false;
  delegate_->show_message_center_success_ = false;

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  AddNotification("ShowBubbleFails");

  ui_controller_->ShowPopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  bool shown = ui_controller_->ShowMessageCenterBubble();
  EXPECT_FALSE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->HidePopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

}  // namespace ash
