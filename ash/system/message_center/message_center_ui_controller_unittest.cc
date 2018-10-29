// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_ui_controller.h"

#include <utility>

#include "base/macros.h"
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

 private:
  ~TestNotificationDelegate() override = default;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationDelegate);
};

class MockDelegate : public MessageCenterUiDelegate {
 public:
  MockDelegate() {}
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
  bool ShowMessageCenter(bool show_by_click) override {
    EXPECT_FALSE(popups_visible_);
    return show_popups_success_;
  }
  void HideMessageCenter() override { EXPECT_FALSE(popups_visible_); }

  bool popups_visible_ = false;
  bool show_popups_success_ = true;
  bool show_message_center_success_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

}  // namespace

class MessageCenterUiControllerTest : public testing::Test {
 public:
  MessageCenterUiControllerTest() {}
  ~MessageCenterUiControllerTest() override {}

  void SetUp() override {
    message_center::MessageCenter::Initialize();
    delegate_.reset(new MockDelegate);
    message_center_ = message_center::MessageCenter::Get();
    ui_controller_.reset(new MessageCenterUiController(delegate_.get()));
  }

  void TearDown() override {
    ui_controller_.reset();
    delegate_.reset();
    message_center_ = NULL;
    message_center::MessageCenter::Shutdown();
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
            ASCIIToUTF16("Test Web Notification"),
            ASCIIToUTF16("Notification message body."), gfx::Image(),
            ASCIIToUTF16("www.test.org"), GURL(), notifier_id,
            message_center::RichNotificationData(),
            new TestNotificationDelegate()));
    message_center::Notification* notification_ptr = notification.get();
    message_center_->AddNotification(std::move(notification));
    return notification_ptr;
  }
  std::unique_ptr<MockDelegate> delegate_;
  std::unique_ptr<MessageCenterUiController> ui_controller_;
  message_center::MessageCenter* message_center_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterUiControllerTest);
};

TEST_F(MessageCenterUiControllerTest, BasicMessageCenter) {
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  bool shown =
      ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_TRUE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);

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

  bool shown =
      ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);
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

  ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);
  ui_controller_->HideMessageCenterBubble();
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

TEST_F(MessageCenterUiControllerTest,
       MessageCenterReopenPopupsForSystemPriority) {
  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  std::unique_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          "MessageCenterReopnPopupsForSystemPriority",
          ASCIIToUTF16("Test Web Notification"),
          ASCIIToUTF16("Notification message body."), gfx::Image(),
          ASCIIToUTF16("www.test.org"), GURL(), DummyNotifierId(),
          message_center::RichNotificationData(), NULL /* delegate */));
  notification->SetSystemPriority();
  message_center_->AddNotification(std::move(notification));

  ASSERT_TRUE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  bool shown =
      ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_TRUE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_TRUE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_TRUE(ui_controller_->popups_visible());
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

  bool shown =
      ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_FALSE(shown);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->HideMessageCenterBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->ShowMessageCenterBubble(false /* show_by_click */);

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());

  ui_controller_->HidePopupBubble();

  ASSERT_FALSE(ui_controller_->popups_visible());
  ASSERT_FALSE(ui_controller_->message_center_visible());
}

}  // namespace ash
