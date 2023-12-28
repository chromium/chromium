// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_overflow_view.h"

#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/test/views_test_base.h"

namespace ash {

class NotificationOverflowViewTest : public views::ViewsTestBase {
 public:
  NotificationOverflowViewTest() {}

  NotificationOverflowViewTest(const NotificationOverflowViewTest&) = delete;
  NotificationOverflowViewTest& operator=(const NotificationOverflowViewTest&) =
      delete;

  ~NotificationOverflowViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    notification_overflow_view_ = std::make_unique<NotificationOverflowView>();
  }

  // Adds a notification and returns the string identifier.
  std::string AddNotification() {
    message_center::ProportionalImageView image_view(gfx::Size(16, 16));
    std::string notification_id =
        base::NumberToString(notification_identifier_++);
    notification_overflow_view_->AddIcon(image_view, notification_id);
    return notification_id;
  }

  // Checks whether |expected_notification_icons| notification icons are being
  // shown in the overflow. Does not include the overflow icon.
  void CheckNumberOfNotificationIcons(int expected_notification_icons) {
    int actual_notification_icons = 0;
    for (views::View* v : notification_overflow_view_->GetChildrenInZOrder()) {
      if (!v->GetVisible() || v->GetID() != kNotificationOverflowIconId)
        continue;

      actual_notification_icons++;
    }
    DCHECK_EQ(expected_notification_icons, actual_notification_icons);
  }

  // Returns whether the overflow icon is being shown.
  bool HasOverflowIcon() {
    for (views::View* v : notification_overflow_view_->GetChildrenInZOrder()) {
      if (v->GetID() != kOverflowIconId)
        continue;

      return v->GetVisible();
    }
    return false;
  }

  NotificationOverflowView* notification_overflow_view() {
    return notification_overflow_view_.get();
  }

 private:
  int notification_identifier_ = 0;
  std::unique_ptr<NotificationOverflowView> notification_overflow_view_;
};

// Tests that icons get added and removed when notifications come and go.
TEST_F(NotificationOverflowViewTest, Basic) {
  // Initially no notification icons should be shown.
  CheckNumberOfNotificationIcons(0);
  EXPECT_FALSE(HasOverflowIcon());

  // Add a notification, an icon should be created.
  std::string id = AddNotification();
  CheckNumberOfNotificationIcons(1);
  EXPECT_FALSE(HasOverflowIcon());

  // Try to remove a notification that doesn't exist. Nothing should change.
  notification_overflow_view()->RemoveIcon("non-existent-notification-id");
  CheckNumberOfNotificationIcons(1);
  EXPECT_FALSE(HasOverflowIcon());

  // Remove the notification that was added earlier, the icon should be removed.
  notification_overflow_view()->RemoveIcon(id);
  CheckNumberOfNotificationIcons(0);
  EXPECT_FALSE(HasOverflowIcon());
}

// Tests that the overflow icon gets added when more than |kMaxOverflowIcons|
// icons are added.
TEST_F(NotificationOverflowViewTest, OverflowIcon) {
  // Add notifications until just before overflow.
  for (int i = 0; i < kMaxOverflowIcons; ++i) {
    AddNotification();
    CheckNumberOfNotificationIcons(i + 1);
    EXPECT_FALSE(HasOverflowIcon());
  }

  // Add one more notification, the overflow icon should appear instead of that
  // new notification.
  AddNotification();

  CheckNumberOfNotificationIcons(kMaxOverflowIcons);
  EXPECT_TRUE(HasOverflowIcon());

  // Remove any notification that was added. The overflow icon should dissapear.
  notification_overflow_view()->RemoveIcon(base::NumberToString(0));
  CheckNumberOfNotificationIcons(kMaxOverflowIcons);
  EXPECT_FALSE(HasOverflowIcon());

  // Add a few more (over |kMaxOverflowIcons|) notifications to show the
  // overflow icon.
  AddNotification();
  AddNotification();
  AddNotification();

  CheckNumberOfNotificationIcons(kMaxOverflowIcons);
  EXPECT_TRUE(HasOverflowIcon());
}

}  // namespace ash
