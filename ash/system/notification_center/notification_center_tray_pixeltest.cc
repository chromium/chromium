// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class NotificationCenterTrayPixelTest : public AshTestBase {
 public:
  NotificationCenterTrayPixelTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->notification_center_tray());
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the UI of the notification center tray when connecting a secondary
// display while two notification icons are present. This was added for
// b/284313750.
TEST_F(NotificationCenterTrayPixelTest,
       NotificationTrayOnSecondaryDisplayWithTwoNotificationIcons) {
  // Add two pinned notifications to make two notification icons show up in the
  // notification center tray.
  test_api()->AddPinnedNotification();
  test_api()->AddPinnedNotification();

  // Add a secondary display.
  UpdateDisplay("800x799,800x799");
  auto secondary_display_id = display_manager()->GetDisplayAt(1).id();
  ASSERT_TRUE(test_api()->GetTrayOnDisplay(secondary_display_id));

  // Check the UI of the notification center tray on the secondary display.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnSecondaryScreen(
      "check_view", /*revision_number=*/2,
      test_api()->GetTrayOnDisplay(secondary_display_id)));
}

}  // namespace ash
