// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/popups_only_ui_controller.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotifierId;

namespace {

class PopupsOnlyUiControllerTest : public views::test::WidgetTest {
 public:
  PopupsOnlyUiControllerTest() = default;
  PopupsOnlyUiControllerTest(const PopupsOnlyUiControllerTest&) = delete;
  PopupsOnlyUiControllerTest& operator=(const PopupsOnlyUiControllerTest&) =
      delete;
  ~PopupsOnlyUiControllerTest() override = default;

  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    views::test::WidgetTest::SetUp();
    MessageCenter::Initialize();
  }

  void TearDown() override {
    MessageCenter::Get()->RemoveAllNotifications(
        false, MessageCenter::RemoveType::ALL);

    for (views::Widget* widget : GetAllWidgets())
      widget->CloseNow();
    MessageCenter::Shutdown();
    views::test::WidgetTest::TearDown();
  }

 protected:
  void AddNotification(const std::string& id) {
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"Test Web Notification",
        u"Notification message body.", ui::ImageModel(),
        u"Some Chrome extension", GURL("chrome-extension://abbccedd"),
        NotifierId(message_center::NotifierType::APPLICATION, id),
        message_center::RichNotificationData(), nullptr);

    MessageCenter::Get()->AddNotification(std::move(notification));
  }

  void UpdateNotification(const std::string& id) {
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        u"Updated Test Web Notification", u"Notification message body.",
        ui::ImageModel(), u"Some Chrome extension",
        GURL("chrome-extension://abbccedd"),
        NotifierId(message_center::NotifierType::APPLICATION, id),
        message_center::RichNotificationData(), nullptr);

    MessageCenter::Get()->UpdateNotification(id, std::move(notification));
  }

  void RemoveNotification(const std::string& id) {
    MessageCenter::Get()->RemoveNotification(id, false);
  }

  bool HasNotification(const std::string& id) {
    return !!MessageCenter::Get()->FindVisibleNotificationById(id);
  }
};

TEST_F(PopupsOnlyUiControllerTest, WebNotificationPopupBubble) {
  auto ui_controller = std::make_unique<PopupsOnlyUiController>();

  // Adding a notification should show the popup bubble.
  AddNotification("id1");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("id2");
  UpdateNotification("id2");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("id1");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("id2");
  EXPECT_FALSE(ui_controller->popups_visible());
}

TEST_F(PopupsOnlyUiControllerTest, ManyPopupNotifications) {
  auto ui_controller = std::make_unique<PopupsOnlyUiController>();

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add =
      message_center::kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("id%d", static_cast<int>(i));
    AddNotification(id);
  }
  EXPECT_TRUE(ui_controller->popups_visible());
  MessageCenter* message_center = MessageCenter::Get();
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  message_center::NotificationList::PopupNotifications popups =
      message_center->GetPopupNotifications();
  EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
}

}  // namespace
