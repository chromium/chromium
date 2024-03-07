// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/pine_items_container_view.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/app_constants/constants.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_utils.h"

namespace ash {

class PineTest : public AshTestBase {
 public:
  PineTest() { switches::SetIgnoreForestSecretKeyForTest(true); }
  PineTest(const PineTest&) = delete;
  PineTest& operator=(const PineTest&) = delete;
  ~PineTest() override { switches::SetIgnoreForestSecretKeyForTest(false); }

  void StartPineOverviewSession(std::unique_ptr<PineContentsData> data) {
    Shell::Get()->pine_controller()->MaybeStartPineOverviewSession(
        std::move(data));
    WaitForOverviewEntered();

    OverviewSession* overview_session =
        OverviewController::Get()->overview_session();
    ASSERT_TRUE(overview_session);
    EXPECT_EQ(OverviewEnterExitType::kPine,
              overview_session->enter_exit_overview_type());

    // Check that the pine widget exists.
    OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(grid);
    auto* pine_widget = OverviewGridTestApi(grid).pine_widget();
    ASSERT_TRUE(pine_widget);

    const PineContentsView* contents_view =
        views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
    ASSERT_TRUE(contents_view);
    ASSERT_TRUE(contents_view->container_view_for_testing());
  }

  const PineItemsOverflowView* GetOverflowView() const {
    return views::AsViewClass<PineContentsView>(
               OverviewGridTestApi(Shell::GetPrimaryRootWindow())
                   .pine_widget()
                   ->GetContentsView())
        ->container_view_for_testing()
        ->overflow_view_for_testing();
  }

  // Used for testing overview. Returns a vector with `n` chrome browser app
  // ids.
  std::unique_ptr<PineContentsData> MakeTestAppIds(int n) {
    auto data = std::make_unique<PineContentsData>();
    for (int i = 0; i < n; ++i) {
      data->apps_infos.emplace_back(app_constants::kChromeAppId);
    }

    return data;
  }

 private:
  InProcessDataDecoder decoder_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
};

TEST_F(PineTest, Show) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  EXPECT_EQ(OverviewEnterExitType::kPine,
            overview_session->enter_exit_overview_type());
}

TEST_F(PineTest, NoOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for one window.
  StartPineOverviewSession(MakeTestAppIds(1));
  EXPECT_FALSE(GetOverflowView());
}

TEST_F(PineTest, TwoWindowOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for two overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 2));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  EXPECT_EQ(2u, overflow_view->image_view_map_for_testing().size());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x1 layout.
  EXPECT_EQ(2u, overflow_view->top_row_view_for_testing()->children().size());
  EXPECT_EQ(0u,
            overflow_view->bottom_row_view_for_testing()->children().size());
}

TEST_F(PineTest, ThreeWindowOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for three overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 3));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  EXPECT_EQ(3u, overflow_view->image_view_map_for_testing().size());

  // The top row should have one element, and the bottom row should have two
  // elements, in order to form a triangular layout.
  EXPECT_EQ(1u, overflow_view->top_row_view_for_testing()->children().size());
  EXPECT_EQ(2u,
            overflow_view->bottom_row_view_for_testing()->children().size());
}

TEST_F(PineTest, FourWindowOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for four overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 4));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  EXPECT_EQ(4u, overflow_view->image_view_map_for_testing().size());

  // The top and bottom rows should have two elements each, in order to form a
  // 2x2 layout.
  EXPECT_EQ(2u, overflow_view->top_row_view_for_testing()->children().size());
  EXPECT_EQ(2u,
            overflow_view->bottom_row_view_for_testing()->children().size());
}

TEST_F(PineTest, FivePlusWindowOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for five overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 5));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);

  // The image view map should only have three elements as the fourth slot is
  // saved for a count of the remaining windows.
  EXPECT_EQ(3u, overflow_view->image_view_map_for_testing().size());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x2 layout.
  EXPECT_EQ(2u, overflow_view->top_row_view_for_testing()->children().size());
  EXPECT_EQ(2u,
            overflow_view->bottom_row_view_for_testing()->children().size());
}

// Tests that the pine screenshot should not be shown if it has different
// orientation as the display will show it.
TEST_F(PineTest, NoScreenshotWithDifferentDisplayOrientation) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& file_path =
      temp_dir.GetPath().AppendASCII("test_pine.png");
  SetPineImagePathForTest(file_path);

  // Take a screenshot of the display that in landscape orientation and save it
  // to the pine image path.
  TakePrimaryDisplayScreenshotAndSave(file_path);
  int64_t file_size = 0;
  ASSERT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_GT(file_size, 0);

  // Rotate the display and trigger the accelerator to show the pine dialog.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  auto* pine_controller = Shell::Get()->pine_controller();
  pine_controller->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();
  const PineContentsData* pine_contents_data =
      pine_controller->pine_contents_data();
  ASSERT_TRUE(pine_contents_data);
  // The image inside `PineContentsData` should be null when the landscape image
  // is going to be shown inside a display in the portrait orientation.
  EXPECT_TRUE(pine_contents_data->image.isNull());
}

}  // namespace ash
