// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/shell.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/templates/saved_desk_test_helper.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/pine_item_view.h"
#include "ash/wm/window_restore/pine_items_container_view.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"
#include "ash/wm/window_restore/pine_test_api.h"
#include "ash/wm/window_restore/pine_test_base.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/app_constants/constants.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace ash {

class PineTest : public PineTestBase {
 public:
  PineTest() = default;
  PineTest(const PineTest&) = delete;
  PineTest& operator=(const PineTest&) = delete;
  ~PineTest() override = default;

  void StartPineOverviewSession(std::unique_ptr<PineContentsData> data) {
    Shell::Get()->pine_controller()->MaybeStartPineOverviewSession(
        std::move(data));
    WaitForOverviewEntered();

    OverviewSession* overview_session =
        OverviewController::Get()->overview_session();
    ASSERT_TRUE(overview_session);

    // Check that the pine widget exists.
    OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(grid);
    auto* pine_widget = OverviewGridTestApi(grid).pine_widget();
    ASSERT_TRUE(pine_widget);

    const PineContentsView* contents_view =
        views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
    ASSERT_TRUE(contents_view);
    auto contents_view_test_api = PineContentsViewTestApi(contents_view);
    ASSERT_TRUE(contents_view_test_api.items_container_view() ||
                contents_view_test_api.screenshot_icon_row_view());
  }

  const PineContentsView* GetContentsView() const {
    return views::AsViewClass<PineContentsView>(
        OverviewGridTestApi(Shell::GetPrimaryRootWindow())
            .pine_widget()
            ->GetContentsView());
  }

  const PineItemsOverflowView* GetOverflowView() const {
    return PineContentsViewTestApi(GetContentsView()).overflow_view();
  }

  const PineScreenshotIconRowView* GetScreenshotIconRowView() const {
    return PineContentsViewTestApi(GetContentsView())
        .screenshot_icon_row_view();
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

  // TODO(minch): Make pine_contents_data->image can be altered, for example,
  // some dummy image to make the test more unit-testy.
  // Takes a screenshot of the entire display and save it to the given
  // `file_path`, which is also set as the path to store the pine screenshot.
  void TakeAndSavePineScreenshot(const base::FilePath& file_path) {
    SetPineImagePathForTest(file_path);

    TakePrimaryDisplayScreenshotAndSave(file_path);
    int64_t file_size = 0;
    ASSERT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, 0);
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  InProcessDataDecoder decoder_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
  static base::Time fake_time_;
};

base::Time PineTest::fake_time_;

TEST_F(PineTest, StartOverviewPineSession) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();
  StartPineOverviewSession(MakeTestAppIds(1));
}

TEST_F(PineTest, NoOverflow) {
  // Start a Pine session with restore data for one window.
  StartPineOverviewSession(MakeTestAppIds(1));
  EXPECT_FALSE(GetOverflowView());
}

TEST_F(PineTest, TwoWindowOverflow) {
  // Start a Pine session with restore data for two overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 2));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  PineItemsOverflowViewTestApi test_api(overflow_view);
  EXPECT_EQ(2u, test_api.image_views_count());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x1 layout.
  EXPECT_EQ(2u, test_api.top_row_view_children_count());
  EXPECT_EQ(0u, test_api.bottom_row_view_children_count());
}

TEST_F(PineTest, ThreeWindowOverflow) {
  // Start a Pine session with restore data for three overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 3));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  PineItemsOverflowViewTestApi test_api(overflow_view);
  EXPECT_EQ(3u, test_api.image_views_count());

  // The top row should have one element, and the bottom row should have two
  // elements, in order to form a triangular layout.
  EXPECT_EQ(1u, test_api.top_row_view_children_count());
  EXPECT_EQ(2u, test_api.bottom_row_view_children_count());
}

TEST_F(PineTest, FourWindowOverflow) {
  // Start a Pine session with restore data for four overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 4));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  PineItemsOverflowViewTestApi test_api(overflow_view);
  EXPECT_EQ(4u, test_api.image_views_count());

  // The top and bottom rows should have two elements each, in order to form a
  // 2x2 layout.
  EXPECT_EQ(2u, test_api.top_row_view_children_count());
  EXPECT_EQ(2u, test_api.bottom_row_view_children_count());
}

TEST_F(PineTest, FivePlusWindowOverflow) {
  auto data = std::make_unique<PineContentsData>();
  data->last_session_crashed = false;

  // Start a Pine session with restore data for five overflow windows.
  StartPineOverviewSession(MakeTestAppIds(pine::kOverflowMinThreshold + 5));

  const PineItemsOverflowView* overflow_view = GetOverflowView();
  ASSERT_TRUE(overflow_view);
  PineItemsOverflowViewTestApi test_api(overflow_view);

  // The image view map should only have three elements as the fourth slot is
  // saved for a count of the remaining windows.
  EXPECT_EQ(3u, test_api.image_views_count());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x2 layout.
  EXPECT_EQ(2u, test_api.top_row_view_children_count());
  EXPECT_EQ(2u, test_api.bottom_row_view_children_count());
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
  TakeAndSavePineScreenshot(temp_dir.GetPath().AppendASCII("test_pine.png"));

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

TEST_F(PineTest, ScreenshotIconRowMaxElements) {
  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TakeAndSavePineScreenshot(temp_dir.GetPath().AppendASCII("test_pine.png"));

  // Starts the session with the maximum number of elements that can be shown
  // inside the icon row.
  StartPineOverviewSession(MakeTestAppIds(pine::kScreenshotIconRowMaxElements));
  const PineContentsData* pine_contents_data =
      Shell::Get()->pine_controller()->pine_contents_data();
  EXPECT_TRUE(pine_contents_data && !pine_contents_data->image.isNull());
  const PineContentsView* contents_view = GetContentsView();
  // Screenshot icon row should be shown instead of the list view when there is
  // a screenshot.
  EXPECT_FALSE(PineContentsViewTestApi(contents_view).items_container_view());
  const PineScreenshotIconRowView* screenshot_icon_row_view =
      GetScreenshotIconRowView();
  EXPECT_TRUE(screenshot_icon_row_view);
  // The icon row should show all the elements and all of them should be shown
  // as icons.
  EXPECT_EQ(5u, screenshot_icon_row_view->children().size());
  EXPECT_EQ(5u, PineScreenshotIconRowViewTestApi(screenshot_icon_row_view)
                    .image_views_count());
}

TEST_F(PineTest, ScreenshotIconRowExceedMaxElements) {
  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TakeAndSavePineScreenshot(temp_dir.GetPath().AppendASCII("test_pine.png"));

  // Starts the session with more elements that can be shown inside the icon
  // row.
  StartPineOverviewSession(
      MakeTestAppIds(pine::kScreenshotIconRowMaxElements + 2));
  const PineContentsData* pine_contents_data =
      Shell::Get()->pine_controller()->pine_contents_data();
  EXPECT_TRUE(pine_contents_data && !pine_contents_data->image.isNull());
  const PineContentsView* contents_view = GetContentsView();
  // Screenshot icon row should be shown instead of the list view when there is
  // a screenshot.
  EXPECT_FALSE(PineContentsViewTestApi(contents_view).items_container_view());
  const PineScreenshotIconRowView* screenshot_icon_row_view =
      GetScreenshotIconRowView();
  EXPECT_TRUE(screenshot_icon_row_view);
  // The icon row should still have at most 5 number of items, but only 4 of
  // them should be icons. The last one should be a count label.
  EXPECT_EQ(4u, PineScreenshotIconRowViewTestApi(screenshot_icon_row_view)
                    .image_views_count());
  EXPECT_EQ(5u, screenshot_icon_row_view->children().size());
}

// Tests that based on preferences (shown count, and last shown time), the nudge
// may or may not be shown.
TEST_F(PineTest, NudgePreferences) {
  SetFakeNow(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides time_override(
      &PineTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto* pine_controller = Shell::Get()->pine_controller();
  auto* anchored_nudge_manager = Shell::Get()->anchored_nudge_manager();

  auto test_start_and_end_overview = [&pine_controller,
                                      &anchored_nudge_manager]() {
    // Reset the nudge if it's currently showing.
    anchored_nudge_manager->Cancel(kEducationNudgeId);
    pine_controller->MaybeStartPineOverviewSessionDevAccelerator();
    WaitForOverviewEntered();
    ToggleOverview();
  };

  // Start pine session, then end overview. Test we show the nudge.
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));

  // Start and end overview. This does not show the nudge as 24 hours have not
  // elapsed since the nudge was shown.
  test_start_and_end_overview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));

  // Start and end overview after waiting 25 hours. The nudge should now show
  // for the second time.
  SetFakeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));

  // Show the nudge for a third time. This will be the last time it is shown.
  SetFakeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));

  // Advance the clock and attempt to show the nudge for a fourth time. Verify
  // that it will not show.
  SetFakeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));
}

// Tests that we only show the nudge for pine overview.
TEST_F(PineTest, NudgePine) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();
  ToggleOverview();
  auto* anchored_nudge_manager = Shell::Get()->anchored_nudge_manager();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));
  anchored_nudge_manager->Cancel(kEducationNudgeId);

  // Reset `pine_contents_data` so we start normal overview.
  PineTestApi().SetPineContentsDataForTesting(nullptr);

  // Start and end overview normally. Test we don't show the nudge.
  ToggleOverview();
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(OverviewGridTestApi(overview_grid).pine_widget());
  ToggleOverview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(kEducationNudgeId));
}

// Tests the onboarding metrics are recorded correctly.
TEST_F(PineTest, OnboardingMetrics) {
  base::HistogramTester histogram_tester;

  // The pref is set to false in tests by default.
  GetTestPrefService()->SetBoolean(prefs::kShouldShowPineOnboarding, true);

  // Verify initial histogram counts.
  histogram_tester.ExpectTotalCount(kPineOnboardingHistogram, 0);

  // Press "Accept". Test we increment `true`.
  auto* pine_controller = Shell::Get()->pine_controller();
  pine_controller->MaybeShowPineOnboardingMessage(
      /*restore_on=*/false);
  auto* dialog = PineTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetAcceptButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  histogram_tester.ExpectBucketCount(kPineOnboardingHistogram,
                                     /*sample=*/true,
                                     /*expected_count=*/1);
  GetTestPrefService()->SetBoolean(prefs::kShouldShowPineOnboarding, true);

  // Press "Cancel". Test we increment `false`.
  pine_controller->MaybeShowPineOnboardingMessage(
      /*restore_on=*/false);
  dialog = PineTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetCancelButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  histogram_tester.ExpectBucketCount(kPineOnboardingHistogram,
                                     /*sample=*/false,
                                     /*expected_count=*/1);
  GetTestPrefService()->SetBoolean(prefs::kShouldShowPineOnboarding, true);

  // Verify total counts.
  histogram_tester.ExpectTotalCount(kPineOnboardingHistogram, 2);

  // Show the onboarding dialog with 'Restore' on. Test we don't record.
  pine_controller->MaybeShowPineOnboardingMessage(
      /*restore_on=*/true);
  dialog = PineTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetAcceptButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  WaitForOverviewEntered();
  histogram_tester.ExpectTotalCount(kPineOnboardingHistogram, 2);
}

// Tests that if we exit overview without clicking the restore or cancel
// buttons, the pine widget gets shown when entering overview next.
TEST_F(PineTest, ToggleOverviewToExit) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_TRUE(OverviewGridTestApi(overview_grid).pine_widget());

  // Exit overview by without clicking the restore or cancel buttons.
  ToggleOverview();
  ASSERT_FALSE(OverviewController::Get()->overview_session());

  ToggleOverview();
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_TRUE(OverviewGridTestApi(overview_grid).pine_widget());
}

TEST_F(PineTest, ClickRestoreToExit) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  views::Widget* pine_widget = OverviewGridTestApi(overview_grid).pine_widget();
  ASSERT_TRUE(pine_widget);

  // Exit overview by clicking the restore or cancel buttons.
  const PillButton* restore_button =
      PineContentsViewTestApi(
          views::AsViewClass<PineContentsView>(pine_widget->GetContentsView()))
          .restore_button();
  LeftClickOn(restore_button);
  ASSERT_FALSE(OverviewController::Get()->overview_session());

  ToggleOverview();
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_FALSE(OverviewGridTestApi(overview_grid).pine_widget());
}

TEST_F(PineTest, PineItemView) {
  // Test when the tab count is within regular limits.
  auto item_view = std::make_unique<PineItemView>(
      u"TEST", std::vector<GURL>{GURL(), GURL(), GURL(), GURL()}, 4u);
  EXPECT_EQ(PineItemViewTestApi(item_view.get())
                .favicon_container_view_for_testing()
                ->children()
                .size(),
            4u);
  item_view.reset();

  // Test the when the tab count has overflow.
  item_view = std::make_unique<PineItemView>(
      u"TEST", std::vector<GURL>{GURL(), GURL(), GURL(), GURL(), GURL()}, 10u);
  EXPECT_EQ(PineItemViewTestApi(item_view.get())
                .favicon_container_view_for_testing()
                ->children()
                .size(),
            5u);
}

// Tests that the pine dialog remains in the center after zooming the display up
// or down.
TEST_F(PineTest, ZoomDisplay) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  aura::Window* root = Shell::GetPrimaryRootWindow();
  OverviewGrid* overview_grid = GetOverviewGridForRoot(root);
  ASSERT_TRUE(overview_grid);
  views::Widget* pine_widget = OverviewGridTestApi(overview_grid).pine_widget();
  ASSERT_TRUE(pine_widget);
  const gfx::Rect& initial_bounds = pine_widget->GetWindowBoundsInScreen();

  // Checks the widget bounds. The x should be exactly centered in the display,
  // the y is near the center and the size remains the same.
  auto verify_widget_bounds = [&root, &pine_widget,
                               &initial_bounds](const std::string& test_name) {
    SCOPED_TRACE(test_name);
    const gfx::Rect root_bounds = root->GetBoundsInScreen();
    const gfx::Rect widget_bounds = pine_widget->GetWindowBoundsInScreen();
    EXPECT_EQ(root_bounds.CenterPoint().x(), widget_bounds.CenterPoint().x());
    EXPECT_LT(widget_bounds.CenterPoint().y(), root_bounds.CenterPoint().y());
    EXPECT_GT(widget_bounds.CenterPoint().y(),
              root_bounds.CenterPoint().y() - 20);
    EXPECT_EQ(initial_bounds.size(), widget_bounds.size());
  };

  // Zoom up twice and down once and verify the bounds of the pine widget at all
  // stages.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int64_t display_id = WindowTreeHostManager::GetPrimaryDisplayId();
  display_manager->ZoomDisplay(display_id, /*up=*/true);
  verify_widget_bounds("Zoom 1, up");
  display_manager->ZoomDisplay(display_id, /*up=*/true);
  verify_widget_bounds("Zoom 2, up");
  display_manager->ZoomDisplay(display_id, /*up=*/false);
  verify_widget_bounds("Zoom 2, down");
}

// Tests that the pine dialog gets hidden when we show the saved desk library.
TEST_F(PineTest, ShowSavedDeskLibrary) {
  // Add one entry for the saved desk button to show up.
  ash_test_helper()->saved_desk_test_helper()->WaitForDeskModels();
  AddSavedDeskEntry(ash_test_helper()->saved_desk_test_helper()->desk_model(),
                    base::Uuid::GenerateRandomV4(), "saved_desk",
                    base::Time::Now(), DeskTemplateType::kSaveAndRecall);

  // Start a pine overview session.
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  views::Widget* pine_widget =
      OverviewGridTestApi(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow()))
          .pine_widget();
  ASSERT_TRUE(pine_widget);

  // Click the library button and test that the dialog has zero opacity.
  const views::Button* library_button = GetLibraryButton();
  ASSERT_TRUE(library_button);
  LeftClickOn(library_button);
  EXPECT_EQ(0.f, pine_widget->GetLayer()->GetTargetOpacity());
}

}  // namespace ash
