// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_container.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class UnifiedMediaControlsContainerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  UnifiedMediaControlsContainerTest() = default;
  ~UnifiedMediaControlsContainerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kQsRevamp,
                                              IsQsRevampEnabled());

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

  QuickSettingsView* quick_settings_view() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->quick_settings_view();
  }

  UnifiedMediaControlsContainer* media_controls_container() {
    return features::IsQsRevampEnabled()
               ? quick_settings_view()->media_controls_container_for_testing()
               : system_tray_view()->media_controls_container_for_testing();
  }

  bool IsQsRevampEnabled() { return GetParam(); }

  void ShowMediaControls() {
    features::IsQsRevampEnabled() ? quick_settings_view()->ShowMediaControls()
                                  : system_tray_view()->ShowMediaControls();
  }

  void ShowDetailedView() {
    auto view = std::make_unique<views::View>();
    features::IsQsRevampEnabled()
        ? quick_settings_view()->SetDetailedView(std::move(view))
        : system_tray_view()->SetDetailedView(std::move(view));
  }

  void ResetDetailedView() {
    features::IsQsRevampEnabled() ? quick_settings_view()->ResetDetailedView()
                                  : system_tray_view()->ResetDetailedView();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedMediaControlsContainerTest,
                         testing::Bool());

TEST_P(UnifiedMediaControlsContainerTest, DoNotShowControlsWhenInDetailedView) {
  // Navigate to a dummy detailed view.
  ShowDetailedView();

  // Simulate media playing, container should still be hidden.
  ShowMediaControls();
  EXPECT_FALSE(media_controls_container()->GetVisible());

  // Return back to main menu, now media controls should show.
  ResetDetailedView();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

TEST_P(UnifiedMediaControlsContainerTest, HideControlsWhenSystemMenuCollapse) {
  // Quick settings is not collapsable.
  if (features::IsQsRevampEnabled()) {
    return;
  }

  EXPECT_FALSE(media_controls_container()->GetVisible());
  system_tray_view()->SetExpandedAmount(0.0f);

  // Simulate media playing, container should be hidden since menu is collapsed.
  system_tray_view()->ShowMediaControls();
  EXPECT_FALSE(media_controls_container()->GetVisible());

  // Controls should be shown as the menu is expanding back to normal state.
  system_tray_view()->SetExpandedAmount(0.1f);
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

TEST_P(UnifiedMediaControlsContainerTest, ShowMediaControls) {
  // Simulate media playing and media controls should show.
  ShowMediaControls();
  EXPECT_TRUE(media_controls_container()->GetVisible());
}

}  // namespace ash
