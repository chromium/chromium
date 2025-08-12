// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shell.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class ShelfLayoutManagerPixelRTLTest
    : public ShelfLayoutManagerTestBase,
      public testing::WithParamInterface<
          std::tuple</*is_tablet_mode=*/bool, /*enable_system_blur*/ bool>> {
 public:
  ShelfLayoutManagerPixelRTLTest() {
    // Disable kHideShelfControlsInTabletMode to disable contextual nudges.
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHideShelfControlsInTabletMode, false}});
  }

  bool IsTabletMode() const { return std::get<0>(GetParam()); }

  bool IsSystemBlurEnabled() const { return std::get<1>(GetParam()); }

  // ShelfLayoutManagerTestBase:
  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();
    PopulateAppShortcut(5);
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = IsSystemBlurEnabled();
    return init_params;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    TabletMode,
    ShelfLayoutManagerPixelRTLTest,
    testing::Combine(/*is_tablet_mode=*/testing::Bool(),
                     /*enable_system_blur*/ testing::Bool()));

TEST_P(ShelfLayoutManagerPixelRTLTest, AutohideShelfVisibility) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(IsTabletMode());

  // Open a fullscreen test widget so that the shelf will auto-hide.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("shelf_no_auto_hide"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 14 : 0,
      shelf->GetWindow(), shelf->hotseat_widget()));

  // When the auto-hide is set and a window is shown fullscreen, the shelf
  // should not be showing on the screen.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("shelf_auto_hide"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 1 : 0,
      shelf->GetWindow(), shelf->hotseat_widget()));

  // Show the shelf in auto-hide mode.
  if (IsTabletMode()) {
    SwipeUpOnShelf();
  } else {
    MoveMouseToShowAutoHiddenShelf();
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("shelf_show_with_auto_hide"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 15 : 0,
      shelf->GetWindow(), shelf->hotseat_widget()));
}

}  // namespace ash
