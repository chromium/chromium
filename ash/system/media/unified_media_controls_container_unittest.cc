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
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"

namespace ash {

class UnifiedMediaControlsContainerTest : public AshTestBase {
 public:
  UnifiedMediaControlsContainerTest() = default;
  ~UnifiedMediaControlsContainerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsForChromeOS);
    AshTestBase::SetUp();

    // Ensure media tray is not pinned to shelf so that media controls
    // show up in quick settings.
    MediaTray::SetPinnedToShelf(false);

    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->ShowBubble();
  }

  UnifiedSystemTrayView* system_tray_view() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->unified_view();
  }

  UnifiedMediaControlsContainer* media_controls_container() {
    return system_tray_view()->media_controls_container_for_testing();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnifiedMediaControlsContainerTest, DoNotShowControlsWhenInDetailedView) {
  // Navigate to a dummy detailed view.
  system_tray_view()->SetDetailedView(std::make_unique<views::View>());

  // Simulate media playing, container should still be hidden.
  system_tray_view()->ShowMediaControls();
  EXPECT_FALSE(media_controls_container()->GetVisible());

  // Return back to main menu, now media controls should show.
  system_tray_view()->ResetDetailedView();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

TEST_F(UnifiedMediaControlsContainerTest, HideControlsWhenSystemMenuCollapse) {
  EXPECT_FALSE(media_controls_container()->GetVisible());
  system_tray_view()->SetExpandedAmount(0.0f);

  // Simulate media playing, container should be hidden since menu is collapsed.
  system_tray_view()->ShowMediaControls();
  EXPECT_FALSE(media_controls_container()->GetVisible());

  // Controls should be shown as the menu is expanding back to normal state.
  system_tray_view()->SetExpandedAmount(0.1f);
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

TEST_F(UnifiedMediaControlsContainerTest, ShowMediaControls) {
  // Simulate media playing and media controls should show.
  system_tray_view()->ShowMediaControls();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

}  // namespace ash
