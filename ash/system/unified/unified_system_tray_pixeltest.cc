// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/system/unified/unified_system_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

class UnifiedSystemTrayPixelTest
    : public AshTestBase,
      public ::testing::WithParamInterface</*IsJellyEnabled()*/ bool> {
 public:
  UnifiedSystemTrayPixelTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kQsRevamp, false},
         {chromeos::features::kJelly, IsJellyEnabled()}});
  }
  UnifiedSystemTrayPixelTest(const UnifiedSystemTrayPixelTest&) = delete;
  UnifiedSystemTrayPixelTest& operator=(const UnifiedSystemTrayPixelTest&) =
      delete;
  ~UnifiedSystemTrayPixelTest() override = default;

  bool IsJellyEnabled() { return GetParam(); }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 protected:
  const std::string AddSimpleNotification() {
    const std::string id = base::NumberToString(id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateSystemNotificationPtr(
            message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test title",
            u"test message",
            /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
            message_center::NotifierId(),
            message_center::RichNotificationData(), /*delegate=*/nullptr,
            /*small_image=*/gfx::VectorIcon(),
            message_center::SystemNotificationWarningLevel::NORMAL));
    return id;
  }

  NotificationCounterView* GetNotificationCounter() {
    return GetPrimaryUnifiedSystemTray()
        ->notification_icons_controller()
        ->notification_counter_view();
  }

  size_t GetNotificationCount() {
    return message_center::MessageCenter::Get()->NotificationCount();
  }

 private:
  int id_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IsJellyEnabled,
                         UnifiedSystemTrayPixelTest,
                         testing::Bool());

// Tests the notification counter UI for the following cases:
//   - one notification
//   - more than the max number of notifications
TEST_P(UnifiedSystemTrayPixelTest, NotificationCounter) {
  // Add a single notification.
  AddSimpleNotification();
  ASSERT_EQ(1u, GetNotificationCount());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "one_notification", /*revision_number=*/0, GetNotificationCounter()));

  // Add the max number of notifications. Given the existing notification, the
  // total notification count should now be one more than the max.
  for (size_t i = 0; i < kTrayNotificationMaxCount; i++) {
    AddSimpleNotification();
  }
  ASSERT_EQ(kTrayNotificationMaxCount + 1, GetNotificationCount());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "more_than_max_notifications", /*revision_number=*/1,
      GetNotificationCounter()));
}

}  // namespace ash
