// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scalable_iph/wallpaper_ash_notification_view.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

class WallpaperAshNotificationViewTest : public AshTestBase {
 public:
  WallpaperAshNotificationViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~WallpaperAshNotificationViewTest() override = default;

 protected:
  message_center::Notification CreateNotification() {
    message_center::RichNotificationData rich_notification_data;
    return message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id",
        u"test_title", u"test message", ui::ImageModel(),
        /*display_source=*/std::u16string(), GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, "notifier_id",
            NotificationCatalogName::kScalableIphNotification),
        rich_notification_data, new message_center::NotificationDelegate());
  }

  views::View* GetImageContainerView(WallpaperAshNotificationView* view) {
    return view->image_container_view();
  }
};

TEST_F(WallpaperAshNotificationViewTest, NotificationHasFourPreviews) {
  auto notification = CreateNotification();
  auto notification_view =
      WallpaperAshNotificationView::CreateWithPreview(notification,
                                                      /*shown_in_popup=*/true);
  auto* wallpaper_notification_view =
      static_cast<WallpaperAshNotificationView*>(notification_view.get());
  views::View* image_container_view =
      GetImageContainerView(wallpaper_notification_view);
  // Notification should contain one preview view, which has four images.
  EXPECT_EQ(1u, image_container_view->children().size());
  EXPECT_EQ(4u, image_container_view->children()[0]->children().size());
}

}  // namespace ash
