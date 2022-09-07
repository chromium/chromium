// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash {

namespace {

void AddNotification(const std::string& notification_id,
                     bool is_pinned = false) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.pinned = is_pinned;
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"test_title", u"test message", ui::ImageModel(),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     "app"),
          rich_notification_data, new message_center::NotificationDelegate()));
}

}  // namespace

class NotificationCounterViewTest : public AshTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  NotificationCounterViewTest() = default;
  NotificationCounterViewTest(const NotificationCounterViewTest&) = delete;
  NotificationCounterViewTest& operator=(const NotificationCounterViewTest&) =
      delete;
  ~NotificationCounterViewTest() override = default;

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
    notification_counter_view_ =
        notification_icons_controller_->notification_counter_view();
  }

  bool IsScalableStatusAreaEnabled() { return GetParam(); }

  void TearDown() override {
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

 protected:
  NotificationCounterView* notification_counter_view() {
    return notification_counter_view_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
  NotificationCounterView* notification_counter_view_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NotificationCounterViewTest,
                         testing::Bool() /* IsScalableStatusAreaEnabled() */);

TEST_P(NotificationCounterViewTest, CountForDisplay) {
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

TEST_P(NotificationCounterViewTest, HiddenNotificationCount) {
  // Not visible when count == 0.
  notification_counter_view()->Update();
  EXPECT_EQ(0, notification_counter_view()->count_for_display_for_testing());
  EXPECT_FALSE(notification_counter_view()->GetVisible());

  // Added a pinned notification, counter should not be visible when the feature
  // is enabled.
  AddNotification("1", true /* is_pinned */);
  notification_counter_view()->Update();
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            !notification_counter_view()->GetVisible());

  // Added a normal notification.
  AddNotification("2");
  notification_counter_view()->Update();
  int expected_count = IsScalableStatusAreaEnabled() ? 1 : 2;
  EXPECT_TRUE(notification_counter_view()->GetVisible());
  EXPECT_EQ(expected_count,
            notification_counter_view()->count_for_display_for_testing());

  // Added another pinned.
  AddNotification("3", true /* is_pinned */);
  notification_counter_view()->Update();
  expected_count = IsScalableStatusAreaEnabled() ? 1 : 3;
  EXPECT_TRUE(notification_counter_view()->GetVisible());
  EXPECT_EQ(expected_count,
            notification_counter_view()->count_for_display_for_testing());

  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  message_center::MessageCenter::Get()->RemoveNotification("3",
                                                           false /* by_user */);
  notification_counter_view()->Update();
  EXPECT_EQ(1, notification_counter_view()->count_for_display_for_testing());
}

TEST_P(NotificationCounterViewTest, DisplayChanged) {
  AddNotification("1", true /* is_pinned */);
  notification_counter_view()->Update();

  // In medium size screen, the counter should not be displayed since pinned
  // notification icon is shown (if the feature is enabled).
  UpdateDisplay("800x700");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            !notification_counter_view()->GetVisible());

  // The counter should not be shown when we remove the pinned notification.
  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  notification_counter_view()->Update();
  EXPECT_FALSE(notification_counter_view()->GetVisible());

  AddNotification("1", true /* is_pinned */);
  notification_counter_view()->Update();

  // In small display, the counter show be shown with pinned notification.
  UpdateDisplay("600x500");
  EXPECT_TRUE(notification_counter_view()->GetVisible());

  // In large screen size, expected the same behavior like medium screen size.
  UpdateDisplay("1680x800");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            !notification_counter_view()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  notification_counter_view()->Update();
  EXPECT_FALSE(notification_counter_view()->GetVisible());
}

}  // namespace ash
