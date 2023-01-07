// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/gesture_education/gesture_education_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

class GestureEducationNotificationControllerTest : public AshTestBase {
 public:
  GestureEducationNotificationControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kHideShelfControlsInTabletMode}, {});
  }
  ~GestureEducationNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<GestureEducationNotificationController>();
    controller_->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetPrimaryUserPrefService());
    controller_->ResetPrefForTest();

    test_api_ = std::make_unique<TabletModeControllerTestApi>();
  }

  void TearDown() override {
    controller_.reset();
    test_api_.reset();
    AshTestBase::TearDown();
  }

  void SetTabletMode(bool on) {
    if (on)
      test_api_->EnterTabletMode();
    else
      test_api_->LeaveTabletMode();
  }

 protected:
  message_center::Notification* GetNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (message_center::NotificationList::Notifications::const_iterator iter =
             notifications.begin();
         iter != notifications.end(); ++iter) {
      if ((*iter)->id() ==
          GestureEducationNotificationController::kNotificationId)
        return *iter;
    }
    return nullptr;
  }

  void RemoveNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        GestureEducationNotificationController::kNotificationId,
        false /* by_user */);
  }

 private:
  std::unique_ptr<GestureEducationNotificationController> controller_;
  std::unique_ptr<TabletModeControllerTestApi> test_api_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GestureEducationNotificationControllerTest, Notification) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  SetTabletMode(true);

  // Notification generated after first time tablet mode entered.
  EXPECT_TRUE(GetNotification());

  RemoveNotification();

  SetTabletMode(false);
  SetTabletMode(true);

  // No notification should be generated once it has been shown once.
  EXPECT_FALSE(GetNotification());
}

}  // namespace ash
