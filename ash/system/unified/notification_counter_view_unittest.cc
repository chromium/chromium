// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include "ash/media/media_notification_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash {

class NotificationCounterViewTest : public AshTestBase {
 public:
  NotificationCounterViewTest() = default;
  NotificationCounterViewTest(const NotificationCounterViewTest&) = delete;
  NotificationCounterViewTest& operator=(const NotificationCounterViewTest&) =
      delete;
  ~NotificationCounterViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    notification_counter_view_ =
        std::make_unique<NotificationCounterView>(GetPrimaryShelf());
  }

  void TearDown() override {
    notification_counter_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void AddNotification(const std::string& notification_id,
                       const std::string& app_id = "app") {
    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
            base::UTF8ToUTF16("test_title"), base::UTF8ToUTF16("test message"),
            gfx::Image(), /*display_source=*/base::string16(), GURL(),
            message_center::NotifierId(
                message_center::NotifierType::APPLICATION, app_id),
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
  }

  NotificationCounterView* notification_counter_view() {
    return notification_counter_view_.get();
  }

 private:
  std::unique_ptr<NotificationCounterView> notification_counter_view_;
};

TEST_F(NotificationCounterViewTest, CountForDisplay) {
  // Not visible when count == 0.
  notification_counter_view()->Update();
  EXPECT_EQ(0, notification_counter_view()->count_for_display_for_testing());
  EXPECT_FALSE(notification_counter_view()->GetVisible());

  // Count is visible and updates between 1..max+1.
  int max = static_cast<int>(kTrayNotificationMaxCount);
  for (int i = 1; i <= max + 1; i++) {
    AddNotification(base::NumberToString(i));
    notification_counter_view()->Update();
    EXPECT_EQ(i, notification_counter_view()->count_for_display_for_testing());
    EXPECT_TRUE(notification_counter_view()->GetVisible());
  }

  // Count does not change after max+1.
  AddNotification(base::NumberToString(max + 2));
  notification_counter_view()->Update();
  EXPECT_EQ(max + 1,
            notification_counter_view()->count_for_display_for_testing());
  EXPECT_TRUE(notification_counter_view()->GetVisible());
}

// Media notifications are not included when flag is set.
TEST_F(NotificationCounterViewTest, MediaNotifications) {
  notification_counter_view()->Update();
  EXPECT_EQ(0, notification_counter_view()->count_for_display_for_testing());
  AddNotification("1", kMediaSessionNotifierId);
  {
    // Counter should ignore media notifications when feature is enabled.
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(features::kMediaNotificationsCounter);
    notification_counter_view()->Update();
    EXPECT_EQ(0, notification_counter_view()->count_for_display_for_testing());
    EXPECT_FALSE(notification_counter_view()->GetVisible());
  }
  {
    // Counter should show media notifications when feature is disabled.
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(features::kMediaNotificationsCounter);
    notification_counter_view()->Update();
    EXPECT_EQ(1, notification_counter_view()->count_for_display_for_testing());
    EXPECT_TRUE(notification_counter_view()->GetVisible());
  }
}

}  // namespace ash
