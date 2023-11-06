// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_container.h"

#include <memory>

#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class UnifiedMediaControlsContainerTest : public AshTestBase {
 public:
  UnifiedMediaControlsContainerTest() = default;
  ~UnifiedMediaControlsContainerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Ensure media tray is not pinned to shelf so that media controls
    // show up in quick settings.
    MediaTray::SetPinnedToShelf(false);

    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->ShowBubble();
  }

  QuickSettingsView* quick_settings_view() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->quick_settings_view();
  }

  UnifiedMediaControlsContainer* media_controls_container() {
    return quick_settings_view()->media_controls_container_for_testing();
  }

  void ShowMediaControls() { quick_settings_view()->ShowMediaControls(); }

  void ShowDetailedView() {
    auto view = std::make_unique<views::View>();
    quick_settings_view()->SetDetailedView(std::move(view));
  }

  void ResetDetailedView() { quick_settings_view()->ResetDetailedView(); }
};

TEST_F(UnifiedMediaControlsContainerTest, DoNotShowControlsWhenInDetailedView) {
  // Navigate to a dummy detailed view.
  ShowDetailedView();

  // Simulate media playing, container should still be hidden.
  ShowMediaControls();
  EXPECT_FALSE(media_controls_container()->GetVisible());

  // Return back to main menu, now media controls should show.
  ResetDetailedView();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

TEST_F(UnifiedMediaControlsContainerTest, ShowMediaControls) {
  // Simulate media playing and media controls should show.
  ShowMediaControls();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

}  // namespace ash
