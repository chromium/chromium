// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view_container.h"

#include "ash/system/media/media_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class QuickSettingsMediaViewContainerTest : public NoSessionAshTestBase {
 public:
  QuickSettingsMediaViewContainerTest() = default;
  QuickSettingsMediaViewContainerTest(
      const QuickSettingsMediaViewContainerTest&) = delete;
  QuickSettingsMediaViewContainerTest& operator=(
      const QuickSettingsMediaViewContainerTest&) = delete;
  ~QuickSettingsMediaViewContainerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    MediaTray::SetPinnedToShelf(false);
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  QuickSettingsView* quick_settings_view() {
    return GetPrimaryUnifiedSystemTray()->bubble()->quick_settings_view();
  }

  QuickSettingsMediaViewContainer* media_view_container() {
    return quick_settings_view()->media_view_container_for_testing();
  }
};

TEST_F(QuickSettingsMediaViewContainerTest, ChangeMediaViewVisibility) {
  EXPECT_FALSE(media_view_container()->GetVisible());

  quick_settings_view()->SetShowMediaView(true);
  EXPECT_TRUE(media_view_container()->GetVisible());

  quick_settings_view()->SetShowMediaView(false);
  EXPECT_FALSE(media_view_container()->GetVisible());
}

TEST_F(QuickSettingsMediaViewContainerTest,
       SwitchBetweenMediaViewAndDetailedView) {
  quick_settings_view()->SetShowMediaView(true);
  EXPECT_TRUE(media_view_container()->IsDrawn());

  // Make the quick settings view navigate to a dummy detailed view.
  quick_settings_view()->SetDetailedView(std::make_unique<views::View>());
  EXPECT_FALSE(media_view_container()->IsDrawn());

  // Make the quick settings view navigate back to the main view.
  quick_settings_view()->ResetDetailedView();
  EXPECT_TRUE(media_view_container()->IsDrawn());
}

}  // namespace ash
