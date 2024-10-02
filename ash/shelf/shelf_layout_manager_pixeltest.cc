// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shell.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class ShelfLayoutManagerPixelRTLTest
    : public ShelfLayoutManagerTestBase,
      public testing::WithParamInterface<bool /*is_tablet_mode=*/> {
 public:
  ShelfLayoutManagerPixelRTLTest() {
    // Disable kHideShelfControlsInTabletMode to disable contextual nudges.
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHideShelfControlsInTabletMode, false},
         {chromeos::features::kJelly, true}});
  }

  // ShelfLayoutManagerTestBase:
  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();
    PopulateAppShortcut(5);
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(TabletMode,
                         ShelfLayoutManagerPixelRTLTest,
                         testing::Bool());

TEST_P(ShelfLayoutManagerPixelRTLTest, AutohideShelfVisibility) {
  const bool is_tablet_mode = GetParam();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(is_tablet_mode);

  // Open a fullscreen test widget so that the shelf will auto-hide.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "shelf_no_auto_hide",
      /*revision_number=*/13, shelf->GetWindow(), shelf->hotseat_widget()));

  // When the auto-hide is set and a window is shown fullscreen, the shelf
  // should not be showing on the screen.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "shelf_auto_hide",
      /*revision_number=*/1, shelf->GetWindow(), shelf->hotseat_widget()));

  // Show the shelf in auto-hide mode.
  if (is_tablet_mode)
    SwipeUpOnShelf();
  else
    MoveMouseToShowAutoHiddenShelf();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "shelf_show_with_auto_hide",
      /*revision_number=*/12, shelf->GetWindow(), shelf->hotseat_widget()));
}

}  // namespace ash
