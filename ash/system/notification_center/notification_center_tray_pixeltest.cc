// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/display/manager/display_manager.h"
#include "ui/message_center/message_center.h"

namespace ash {

class NotificationCenterTrayPixelTest : public AshTestBase {
 public:
  NotificationCenterTrayPixelTest() = default;
  NotificationCenterTrayPixelTest(const NotificationCenterTrayPixelTest&) =
      delete;
  NotificationCenterTrayPixelTest& operator=(
      const NotificationCenterTrayPixelTest&) = delete;
  ~NotificationCenterTrayPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
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
  auto* tray = test_api()->GetTrayOnDisplay(secondary_display_id);
  ASSERT_TRUE(tray);

  // Hide any popups that may be showing to reduce the chance that this test
  // flakes. See http://b/306483873 for details.
  tray->ShowBubble();
  tray->CloseBubble();
  ASSERT_FALSE(message_center::MessageCenter::Get()->HasPopupNotifications());

  // Check the UI of the notification center tray on the secondary display.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnSecondaryScreen(
      "check_view", /*revision_number=*/3, tray));
}

}  // namespace ash
