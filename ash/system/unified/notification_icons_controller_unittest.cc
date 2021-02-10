// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/media/media_notification_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class NotificationIconsControllerTest : public AshTestBase {
 public:
  NotificationIconsControllerTest() = default;
  ~NotificationIconsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    tray_ = std::make_unique<UnifiedSystemTray>(GetPrimaryShelf());
    notification_icons_controller_ =
        std::make_unique<NotificationIconsController>(tray_.get());
    notification_icons_controller_->AddNotificationTrayItems(
        tray_->tray_container());
  }

  void TearDown() override {
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

  TrayItemView* separator() {
    return notification_icons_controller_->separator_;
  }

  TrayItemView* hidden_notification_count_view() {
    return notification_icons_controller_->hidden_notification_count_view_;
  }

  std::string AddNotification(bool is_pinned,
                              bool is_critical_warning,
                              const std::string& app_id = "app") {
    std::string id = base::NumberToString(notification_id_++);

    auto warning_level =
        is_critical_warning
            ? message_center::SystemNotificationWarningLevel::CRITICAL_WARNING
            : message_center::SystemNotificationWarningLevel::NORMAL;
    message_center::RichNotificationData rich_notification_data;
    rich_notification_data.pinned = is_pinned;

    message_center::MessageCenter::Get()->AddNotification(
        CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE, id,
            base::UTF8ToUTF16("test_title"), base::UTF8ToUTF16("test message"),
            base::string16() /*display_source */, GURL() /* origin_url */,
            message_center::NotifierId(
                message_center::NotifierType::SYSTEM_COMPONENT, app_id),
            rich_notification_data, nullptr /* delegate */, gfx::VectorIcon(),
            warning_level));
    notification_id_++;

    return id;
  }

 protected:
  int notification_id_ = 0;
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
};

TEST_F(NotificationIconsControllerTest, DisplayChanged) {
  AddNotification(true /* is_pinned */, false /* is_critical_warning */);
  AddNotification(false /* is_pinned */, false /* is_critical_warning */);
  notification_icons_controller_->UpdateHiddenNotificationCounter();

  // Notification icons should be shown in medium screen size.
  UpdateDisplay("800x800");
  EXPECT_TRUE(
      notification_icons_controller_->tray_items().front()->GetVisible());
  EXPECT_TRUE(separator()->GetVisible());
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());

  // Notification icons should not be shown in small screen size.
  UpdateDisplay("600x600");
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().front()->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  // Notification icons should be shown in large screen size.
  UpdateDisplay("1680x800");
  EXPECT_TRUE(
      notification_icons_controller_->tray_items().front()->GetVisible());
  EXPECT_TRUE(separator()->GetVisible());
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());
}

TEST_F(NotificationIconsControllerTest, ShowNotificationIcons) {
  UpdateDisplay("800x800");

  // If there's no notification, no notification icons should be shown.
  EXPECT_FALSE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_FALSE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());

  // Same case for non pinned or non critical warning notification.
  AddNotification(false /* is_pinned */, false /* is_critical_warning */);
  EXPECT_FALSE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_FALSE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());

  // Notification icons should be shown when pinned or critical warning
  // notification is added.
  std::string id0 =
      AddNotification(true /* is_pinned */, false /* is_critical_warning */);
  EXPECT_TRUE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_FALSE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_TRUE(separator()->GetVisible());

  std::string id1 =
      AddNotification(false /* is_pinned */, true /* is_critical_warning */);
  EXPECT_TRUE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_TRUE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_TRUE(separator()->GetVisible());

  // Remove the critical warning notification should make the tray show only one
  // icon.
  message_center::MessageCenter::Get()->RemoveNotification(id1,
                                                           false /* by_user */);
  EXPECT_TRUE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_FALSE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_TRUE(separator()->GetVisible());

  // Remove the pinned notification, no icon is shown.
  message_center::MessageCenter::Get()->RemoveNotification(id0,
                                                           false /* by_user */);
  EXPECT_FALSE(notification_icons_controller_->tray_items()[0]->GetVisible());
  EXPECT_FALSE(notification_icons_controller_->tray_items()[1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
}

TEST_F(NotificationIconsControllerTest, HiddenNotificationCount) {
  UpdateDisplay("800x800");

  // If there's no notification, the counter should be hidden by default.
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  int hidden_notification_num = 5;
  base::string16 expected_text = base::UTF8ToUTF16("+5");

  // The counter should not be shown if no icon is displayed in the tray (a.k.a
  // no important notification).
  for (int i = 0; i < hidden_notification_num; ++i) {
    AddNotification(false /* is_pinned */, false /* is_critical_warning */);
  }
  notification_icons_controller_->UpdateHiddenNotificationCounter();
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  // Added a pinned notification, the counter should now be shown with the
  // expected text.
  std::string id0 =
      AddNotification(true /* is_pinned */, false /* is_critical_warning */);
  notification_icons_controller_->UpdateHiddenNotificationCounter();
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());
  EXPECT_EQ(expected_text,
            hidden_notification_count_view()->label()->GetText());

  // Remove the pinned notification should make the counter switch to hidden.
  message_center::MessageCenter::Get()->RemoveNotification(id0,
                                                           false /* by_user */);
  notification_icons_controller_->UpdateHiddenNotificationCounter();
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());
}

}  // namespace ash
