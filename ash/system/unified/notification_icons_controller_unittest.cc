// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {
const char kBatteryNotificationNotifierId[] = "ash.battery";
const char kUsbNotificationNotifierId[] = "ash.power";
}  // namespace

class NotificationIconsControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  NotificationIconsControllerTest() = default;
  ~NotificationIconsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatureState(features::kScalableStatusArea,
                                              IsScalableStatusAreaEnabled());
    tray_ = std::make_unique<UnifiedSystemTray>(GetPrimaryShelf());
    notification_icons_controller_ =
        std::make_unique<NotificationIconsController>(tray_.get());
    notification_icons_controller_->AddNotificationTrayItems(
        tray_->tray_container());
  }

  bool IsScalableStatusAreaEnabled() { return GetParam(); }

  void TearDown() override {
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

  TrayItemView* separator() {
    return notification_icons_controller_->separator_;
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
            message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test_title",
            u"test message", std::u16string() /*display_source */,
            GURL() /* origin_url */,
            message_center::NotifierId(
                message_center::NotifierType::SYSTEM_COMPONENT, app_id,
                NotificationCatalogName::kTestCatalogName),
            rich_notification_data, nullptr /* delegate */, gfx::VectorIcon(),
            warning_level));
    notification_id_++;

    return id;
  }

 protected:
  int notification_id_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NotificationIconsControllerTest,
                         testing::Bool() /* IsScalableStatusAreaEnabled() */);

TEST_P(NotificationIconsControllerTest, DisplayChanged) {
  AddNotification(true /* is_pinned */, false /* is_critical_warning */);
  AddNotification(false /* is_pinned */, false /* is_critical_warning */);

  // Icons get added from RTL, so we check the end of the vector first.

  // Notification icons should be shown in medium screen size.
  UpdateDisplay("800x700");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_EQ(IsScalableStatusAreaEnabled(), separator()->GetVisible());

  // Notification icons should not be shown in small screen size.
  UpdateDisplay("600x500");
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());

  // Notification icons should be shown in large screen size.
  UpdateDisplay("1680x800");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_EQ(IsScalableStatusAreaEnabled(), separator()->GetVisible());
}

TEST_P(NotificationIconsControllerTest, ShowNotificationIcons) {
  UpdateDisplay("800x700");

  // Icons get added from RTL, so we check the end of the vector first.
  const int end = notification_icons_controller_->tray_items().size() - 1;

  // Ensure that the indexes that will be accessed exist.
  ASSERT_TRUE(notification_icons_controller_->tray_items().size() >= 2);

  // If there's no notification, no notification icons should be shown.
  EXPECT_FALSE(notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_FALSE(
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());

  // Same case for non pinned or non critical warning notification.
  AddNotification(false /* is_pinned */, false /* is_critical_warning */);
  EXPECT_FALSE(notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_FALSE(
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());

  // Notification icons should be shown when pinned or critical warning
  // notification is added.
  std::string id0 =
      AddNotification(true /* is_pinned */, false /* is_critical_warning */);
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_FALSE(
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_EQ(IsScalableStatusAreaEnabled(), separator()->GetVisible());

  std::string id1 =
      AddNotification(false /* is_pinned */, true /* is_critical_warning */);
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_EQ(
      IsScalableStatusAreaEnabled(),
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_EQ(IsScalableStatusAreaEnabled(), separator()->GetVisible());

  // Remove the critical warning notification should make the tray show only one
  // icon.
  message_center::MessageCenter::Get()->RemoveNotification(id1,
                                                           false /* by_user */);
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_FALSE(
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_EQ(IsScalableStatusAreaEnabled(), separator()->GetVisible());

  // Remove the pinned notification, no icon is shown.
  message_center::MessageCenter::Get()->RemoveNotification(id0,
                                                           false /* by_user */);
  EXPECT_FALSE(notification_icons_controller_->tray_items()[end]->GetVisible());
  EXPECT_FALSE(
      notification_icons_controller_->tray_items()[end - 1]->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
}

TEST_P(NotificationIconsControllerTest, NotShowNotificationIcons) {
  UpdateDisplay("800x700");

  // Icons get added from RTL, so we check the end of the vector first.

  EXPECT_FALSE(
      notification_icons_controller_->tray_items().back()->GetVisible());

  AddNotification(true /* is_pinned */, false /* is_critical_warning */,
                  kBatteryNotificationNotifierId);
  // Battery notification should not be shown.
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
  // Notification count does update for this notification.
  notification_icons_controller_->notification_counter_view()->Update();
  EXPECT_EQ(1, notification_icons_controller_->notification_counter_view()
                   ->count_for_display_for_testing());

  AddNotification(true /* is_pinned */, false /* is_critical_warning */,
                  kUsbNotificationNotifierId);
  // Usb charging notification should not be shown.
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
  // Notification count does update for this notification.
  notification_icons_controller_->notification_counter_view()->Update();
  EXPECT_EQ(2, notification_icons_controller_->notification_counter_view()
                   ->count_for_display_for_testing());

  AddNotification(true /* is_pinned */, false /* is_critical_warning */,
                  kVmCameraMicNotifierId);
  // VM camera/mic notification should not be shown.
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().back()->GetVisible());
  EXPECT_FALSE(separator()->GetVisible());
  // Notification count does not update for this notification (since there's
  // another tray item for this).
  notification_icons_controller_->notification_counter_view()->Update();
  EXPECT_EQ(2, notification_icons_controller_->notification_counter_view()
                   ->count_for_display_for_testing());
}

}  // namespace ash
