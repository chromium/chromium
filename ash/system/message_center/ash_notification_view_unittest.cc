// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"

using message_center::Notification;
using message_center::NotificationHeaderView;
using message_center::NotificationView;

namespace ash {

namespace {

constexpr char kDefaultNotificationId[] = "ash notification id";

const gfx::Image CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorGREEN);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

// Create a test notification that is used in the view.
std::unique_ptr<Notification> CreateTestNotification(bool has_image = false) {
  message_center::RichNotificationData data;
  data.settings_button_handler = message_center::SettingsButtonHandler::INLINE;

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_BASE_FORMAT,
      std::string(kDefaultNotificationId), u"title", u"message",
      CreateTestImage(80, 80), u"display source", GURL(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 "extension_id"),
      data, nullptr /* delegate */);
  notification->set_small_image(CreateTestImage(16, 16));

  if (has_image)
    notification->set_image(CreateTestImage(320, 240));

  return notification;
}

}  // namespace

class AshNotificationViewTest : public AshTestBase, public views::ViewObserver {
 public:
  AshNotificationViewTest() = default;
  AshNotificationViewTest(const AshNotificationViewTest&) = delete;
  AshNotificationViewTest& operator=(const AshNotificationViewTest&) = delete;
  ~AshNotificationViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    auto notification = CreateTestNotification();
    notification_view_ = std::make_unique<AshNotificationView>(
        *notification, /*is_popup=*/false);
  }

  void TearDown() override {
    notification_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  views::View* GetExpandButton() { return notification_view_->expand_button_; }

  AshNotificationView* notification_view() { return notification_view_.get(); }

 private:
  std::unique_ptr<AshNotificationView> notification_view_;
};

TEST_F(AshNotificationViewTest, ExpandButtonVisibility) {
  // Expand button should not be shown when it is not expandable.
  EXPECT_FALSE(GetExpandButton()->GetVisible());

  // Expand button should be shown when it is expandable (i.e. there is an
  // image).
  auto notification = CreateTestNotification(true /* has_image */);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_TRUE(GetExpandButton()->GetVisible());
}

}  // namespace ash
