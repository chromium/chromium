// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view_controller.h"

#include "ash/system/media/media_tray.h"
#include "ash/system/media/mock_media_notification_provider.h"
#include "ash/system/media/quick_settings_media_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "components/media_message_center/mock_media_notification_item.h"

namespace ash {

class QuickSettingsMediaViewControllerTest : public AshTestBase {
 public:
  QuickSettingsMediaViewControllerTest() = default;
  QuickSettingsMediaViewControllerTest(
      const QuickSettingsMediaViewControllerTest&) = delete;
  QuickSettingsMediaViewControllerTest& operator=(
      const QuickSettingsMediaViewControllerTest&) = delete;
  ~QuickSettingsMediaViewControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    provider_ = std::make_unique<MockMediaNotificationProvider>();

    MediaTray::SetPinnedToShelf(false);
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    item_ = std::make_unique<testing::NiceMock<
        media_message_center::test::MockMediaNotificationItem>>();
  }

  QuickSettingsMediaViewController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller()
        ->media_view_controller();
  }

  QuickSettingsMediaView* view() {
    return controller()->media_view_for_testing();
  }

  base::WeakPtr<media_message_center::test::MockMediaNotificationItem> item() {
    return item_->GetWeakPtr();
  }

 private:
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;
  std::unique_ptr<MockMediaNotificationProvider> provider_;
};

TEST_F(QuickSettingsMediaViewControllerTest, ShowOrHideMediaItem) {
  const std::string item_id = "item_id";
  EXPECT_EQ(0, static_cast<int>(view()->items_for_testing().size()));

  controller()->ShowMediaItem(item_id, item());
  EXPECT_EQ(1, static_cast<int>(view()->items_for_testing().size()));
  EXPECT_TRUE(view()->items_for_testing().contains(item_id));

  controller()->HideMediaItem(item_id);
  EXPECT_EQ(0, static_cast<int>(view()->items_for_testing().size()));
}

}  // namespace ash
