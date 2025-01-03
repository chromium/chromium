// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/notification_utils.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view.h"
#include "url/gurl.h"

namespace ash {
namespace {
constexpr char kTestNotificationId[] = "TestNotificationId";

std::string GetScreenshotName(const std::string& test_name, bool new_width) {
  return test_name + (new_width ? "_new_width" : "_old_width");
}

}  // namespace

class ScalableIphPixelTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  bool IsNotificationWidthIncreaseEnabled() { return GetParam(); }

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kNotificationWidthIncrease,
        IsNotificationWidthIncreaseEnabled());
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

 protected:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ScalableIphPixelTest,
    /*IsNotificationWidthIncreaseEnabled()=*/testing::Bool());

// To show a notification with no body text, we set an empty string to message
// field. Make sure that it shows our desired UI output.
TEST_P(ScalableIphPixelTest, NotificationNoBodyText) {
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
      GetScreenshotName("scalable_iph_notification_no_body_text",
                        IsNotificationWidthIncreaseEnabled()),
      /*revision_number=*/1, message_view));
}
}  // namespace ash
