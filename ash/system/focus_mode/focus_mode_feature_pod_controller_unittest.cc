// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_utils.h"

namespace ash {

class FocusModeFeaturePodControllerTest : public AshTestBase {
 public:
  FocusModeFeaturePodControllerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFocusMode, features::kQsRevamp},
        /*disabled_features=*/{});
  }
  ~FocusModeFeaturePodControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    CreateFakeFocusModeTile();
  }

  void TearDown() override {
    controller_.reset();
    tile_.reset();
    AshTestBase::TearDown();
  }

  void CreateFakeFocusModeTile() {
    // We need to show the bubble in order to get the
    // `unified_system_tray_controller`.
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    controller_ = std::make_unique<FocusModeFeaturePodController>(
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller());
    tile_ = controller_->CreateTile();
  }

  void ExpectFocusModeDetailedViewShown() {
    TrayDetailedView* detailed_view =
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->quick_settings_view()
            ->GetDetailedViewForTest<TrayDetailedView>();
    ASSERT_TRUE(detailed_view);
    EXPECT_TRUE(views::IsViewClass<FocusModeDetailedView>(detailed_view));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FocusModeFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

// Tests that the tile is normally visible, and is not visible when the screen
// is locked.
TEST_F(FocusModeFeaturePodControllerTest, TileVisibility) {
  // The tile is visible in an active session.
  EXPECT_TRUE(tile_->GetVisible());

  // Locking the screen closes the system tray bubble, so we need to create the
  // tile again.
  GetSessionControllerClient()->LockScreen();
  CreateFakeFocusModeTile();

  // Verify that the tile should not be visible at the lock screen.
  EXPECT_FALSE(tile_->GetVisible());
}

// Tests that pressing the icon works and toggles a Focus Mode Session.
TEST_F(FocusModeFeaturePodControllerTest, PressIconTogglesFocusModeSession) {
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(tile_->GetVisible());
  EXPECT_TRUE(tile_->GetEnabled());

  // Verify that clicking the icon and starting the Focus Mode Session closes
  // the bubble.
  controller_->OnIconPressed();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Recreate the tile since the bubble was closed.
  CreateFakeFocusModeTile();
  EXPECT_TRUE(tile_->IsToggled());

  // End the focus session. This should not close the bubble.
  controller_->OnIconPressed();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(tile_->IsToggled());
}

// Tests that pressing the label works and shows the `FocusModeDetailedView`.
TEST_F(FocusModeFeaturePodControllerTest, PressLabelEntersFocusPanel) {
  controller_->OnLabelPressed();
  ExpectFocusModeDetailedViewShown();
}

}  // namespace ash
