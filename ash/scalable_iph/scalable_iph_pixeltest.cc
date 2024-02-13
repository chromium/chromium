// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/notification_utils.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view.h"
#include "url/gurl.h"

namespace ash {
namespace {
constexpr char kTestNotificationId[] = "TestNotificationId";
}

class ScalableIphPixelTest : public AshTestBase {
 protected:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

// To show a notification with no body text, we set an empty string to message
// field. Make sure that it shows our desired UI output.
TEST_F(ScalableIphPixelTest, NotificationNoBodyText) {
  // TODO(b/323426306): update this test to test logic in ash.
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kTestNotificationId,
          u"NotificationTitle", /*message=*/u"", u"NotificationSourceName",
          GURL(), message_center::NotifierId(),
          message_center::RichNotificationData(),
          /*delegate=*/nullptr, gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->AddNotification(std::move(notification));

  ash::NotificationCenterTestApi notification_center_test_api;
  // Toggle notification bubble UI as the notification is shown on the screen.
  notification_center_test_api.ToggleBubble();
  message_center::MessageView* message_view =
      notification_center_test_api.GetNotificationViewForId(
          kTestNotificationId);
  ASSERT_TRUE(message_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "scalable_iph_notification_no_body_text", /*revision_number=*/0,
      message_view));
}

}  // namespace ash
