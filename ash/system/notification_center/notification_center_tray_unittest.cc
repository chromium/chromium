// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class NotificationCenterTrayTest : public AshTestBase {
 public:
  NotificationCenterTrayTest() = default;
  NotificationCenterTrayTest(const NotificationCenterTrayTest&) = delete;
  NotificationCenterTrayTest& operator=(const NotificationCenterTrayTest&) =
      delete;
  ~NotificationCenterTrayTest() override = default;

  void SetUp() override {
    // Enable quick settings revamp feature.
    scoped_feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});

    AshTestBase::SetUp();

    notification_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                             ->notification_center_tray();
  }

  void TearDown() override {
    notification_tray_ = nullptr;
    AshTestBase::TearDown();
  }

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const std::string& title = "test_title") {
    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, base::UTF8ToUTF16(title),
        u"test message", ui::ImageModel(),
        /*display_source=*/std::u16string(), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
  }

  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateNotification(id));
    return id;
  }

  NotificationCenterTray* GetNotificationCenterTray() {
    return notification_tray_;
  }

 private:
  int id_ = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Owned by `StatusAreaWidget`.
  NotificationCenterTray* notification_tray_ = nullptr;
};

// Test the initial state.
TEST_F(NotificationCenterTrayTest, VisibilityBasedOnAvailableNotifications) {
  EXPECT_FALSE(GetNotificationCenterTray()->GetVisible());

  std::string id = AddNotification();
  EXPECT_TRUE(GetNotificationCenterTray()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification(id, true);

  EXPECT_FALSE(GetNotificationCenterTray()->GetVisible());
}

}  // namespace ash
