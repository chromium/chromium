// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/system/unified/unified_system_tray.h"
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

namespace {

void AddNotification(const std::string& notification_id,
                     bool is_pinned = false) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.pinned = is_pinned;
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          base::UTF8ToUTF16("test_title"), base::UTF8ToUTF16("test message"),
          gfx::Image(), /*display_source=*/base::string16(), GURL(),
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

    if (IsScalableStatusAreaEnabled()) {
      notification_icons_controller_ =
          std::make_unique<NotificationIconsController>(tray_.get());
      notification_icons_controller_->AddNotificationTrayItems(
          tray_->tray_container());
      notification_counter_view_ = std::make_unique<NotificationCounterView>(
          tray_.get(), notification_icons_controller_.get());
    } else {
      notification_counter_view_ =
          std::make_unique<NotificationCounterView>(tray_.get(), nullptr);
    }
  }

  bool IsScalableStatusAreaEnabled() { return GetParam(); }

  void TearDown() override {
    notification_counter_view_.reset();
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

 protected:
  NotificationCounterView* notification_counter_view() {
    return notification_counter_view_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
  std::unique_ptr<NotificationCounterView> notification_counter_view_;
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

TEST_P(NotificationCounterViewTest, DisplayChanged) {
  AddNotification("0", false /* is_pinned */);
  AddNotification("1", true /* is_pinned */);
  notification_counter_view()->Update();

  // In medium size screen, the counter should not be displayed since pinned
  // notification icon is shown (if the feature is enabled).
  UpdateDisplay("800x800");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            !notification_counter_view()->GetVisible());

  // The counter should be shown when we remove the pinned notification.
  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  notification_counter_view()->Update();
  EXPECT_TRUE(notification_counter_view()->GetVisible());

  AddNotification("1", true /* is_pinned */);
  notification_counter_view()->Update();

  // In small display, the counter show be shown with pinned notification.
  UpdateDisplay("600x600");
  EXPECT_TRUE(notification_counter_view()->GetVisible());

  // In large screen size, expected the same behavior like medium screen size.
  UpdateDisplay("1680x800");
  EXPECT_EQ(IsScalableStatusAreaEnabled(),
            !notification_counter_view()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  notification_counter_view()->Update();
  EXPECT_TRUE(notification_counter_view()->GetVisible());
}

class HiddenNotificationCountViewTest : public AshTestBase {
 public:
  HiddenNotificationCountViewTest() = default;
  HiddenNotificationCountViewTest(const HiddenNotificationCountViewTest&) =
      delete;
  HiddenNotificationCountViewTest& operator=(
      const HiddenNotificationCountViewTest&) = delete;
  ~HiddenNotificationCountViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    tray_ = std::make_unique<UnifiedSystemTray>(GetPrimaryShelf());
    notification_icons_controller_ =
        std::make_unique<NotificationIconsController>(tray_.get());
    notification_icons_controller_->AddNotificationTrayItems(
        tray_->tray_container());
    hidden_notification_count_view_ =
        notification_icons_controller_->hidden_notification_count_view();
  }

  void TearDown() override {
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

 protected:
  HiddenNotificationCountView* hidden_notification_count_view() {
    return hidden_notification_count_view_;
  }

 private:
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
  HiddenNotificationCountView* hidden_notification_count_view_;
};

TEST_F(HiddenNotificationCountViewTest, DisplayChanged) {
  AddNotification("0", true /* is_pinned */);
  AddNotification("1", false /* is_pinned */);
  hidden_notification_count_view()->Update();

  // Counter should be shown in medium screen size.
  UpdateDisplay("800x800");
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());

  // Notification icons should not be shown in small screen size.
  UpdateDisplay("600x600");
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  // Notification icons should be shown in large screen size.
  UpdateDisplay("1680x800");
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());
}

TEST_F(HiddenNotificationCountViewTest, HiddenNotificationCount) {
  UpdateDisplay("800x800");

  // If there's no notification, the counter should be hidden by default.
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  int hidden_notification_num = 5;
  base::string16 expected_text = base::UTF8ToUTF16("+5");

  // The counter should not be shown if no icon is displayed in the tray (a.k.a
  // no important notification).
  for (int i = 0; i < hidden_notification_num; ++i) {
    AddNotification(base::NumberToString(i), false /* is_pinned */);
  }
  hidden_notification_count_view()->Update();
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());

  // Added a pinned notification, the counter should now be shown with the
  // expected text.
  AddNotification("5", true /* is_pinned */);
  hidden_notification_count_view()->Update();
  EXPECT_TRUE(hidden_notification_count_view()->GetVisible());
  EXPECT_EQ(expected_text,
            hidden_notification_count_view()->label()->GetText());

  // Remove the pinned notification should make the counter switch to hidden.
  message_center::MessageCenter::Get()->RemoveNotification("5",
                                                           false /* by_user */);
  hidden_notification_count_view()->Update();
  EXPECT_FALSE(hidden_notification_count_view()->GetVisible());
}

}  // namespace ash
