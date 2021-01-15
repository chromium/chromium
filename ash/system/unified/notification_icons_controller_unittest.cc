// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class NotificationIconsControllerTest : public AshTestBase {
 public:
  NotificationIconsControllerTest() = default;
  ~NotificationIconsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    tray_ = std::make_unique<UnifiedSystemTray>(GetPrimaryShelf());
    notification_icons_controller_ =
        std::make_unique<NotificationIconsController>(tray_.get());
    notification_icons_controller_->AddNotificationTrayItems(
        tray_->tray_container());
  }

  void TearDown() override {
    notification_icons_controller_.reset();
    tray_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<UnifiedSystemTray> tray_;
  std::unique_ptr<NotificationIconsController> notification_icons_controller_;
};

TEST_F(NotificationIconsControllerTest, DisplayChanged) {
  // Notification icons should not be shown in small screen size.
  UpdateDisplay("600x600");
  EXPECT_FALSE(
      notification_icons_controller_->tray_items().front()->GetVisible());

  // Notification icons should be shown in medium and large screen size.
  UpdateDisplay("800x800");
  EXPECT_TRUE(
      notification_icons_controller_->tray_items().front()->GetVisible());

  UpdateDisplay("1680x800");
  EXPECT_TRUE(
      notification_icons_controller_->tray_items().front()->GetVisible());
}

}  // namespace ash
