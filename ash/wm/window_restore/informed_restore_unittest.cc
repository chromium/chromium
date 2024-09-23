// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_restore/informed_restore_app_image_view.h"
#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_contents_view.h"
#include "ash/wm/window_restore/informed_restore_context_menu_model.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_restore/informed_restore_item_view.h"
#include "ash/wm/window_restore/informed_restore_items_container_view.h"
#include "ash/wm/window_restore/informed_restore_items_overflow_view.h"
#include "ash/wm/window_restore/informed_restore_screenshot_icon_row_view.h"
#include "ash/wm/window_restore/informed_restore_test_api.h"
#include "ash/wm/window_restore/informed_restore_test_base.h"
#include "ash/wm/window_restore/window_restore_metrics.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/display_util.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display_switches.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace ash {

class InformedRestoreTest : public InformedRestoreTestBase {
 public:
  InformedRestoreTest() = default;
  InformedRestoreTest(const InformedRestoreTest&) = delete;
  InformedRestoreTest& operator=(const InformedRestoreTest&) = delete;
  ~InformedRestoreTest() override = default;

  void SetUp() override {
    // Required for the display rotation to register as a change in orientation.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    InformedRestoreTestBase::SetUp();
  }

  // Starts an overview session with an informed restore dialog created from
  // `data`.
  void StartOverviewSession(std::unique_ptr<InformedRestoreContentsData> data) {
    Shell::Get()
        ->informed_restore_controller()
        ->MaybeStartInformedRestoreSession(std::move(data));
    WaitForOverviewEntered();

    OverviewSession* overview_session =
        OverviewController::Get()->overview_session();
    ASSERT_TRUE(overview_session);

    // Check that the informed restore widget exists.
    OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(grid);
    auto* widget = OverviewGridTestApi(grid).informed_restore_widget();
    ASSERT_TRUE(widget);

    const auto* contents_view = views::AsViewClass<InformedRestoreContentsView>(
        widget->GetContentsView());
    ASSERT_TRUE(contents_view);
    ASSERT_TRUE(
        contents_view->GetViewByID(informed_restore::kPreviewContainerViewID));
  }

  const InformedRestoreContentsView* GetContentsView() const {
    return views::AsViewClass<InformedRestoreContentsView>(
        OverviewGridTestApi(Shell::GetPrimaryRootWindow())
            .informed_restore_widget()
            ->GetContentsView());
  }

  // Returns the count of views with `view_id` starting at the view tree defined
  // by `root`.
  size_t GetViewCount(const views::View* root, int view_id) const {
    size_t count = 0u;

    if (root->GetID() == view_id) {
      count++;
    }

    for (const views::View* child : root->children()) {
      count += GetViewCount(child, view_id);
    }

    return count;
  }

  size_t GetOverflowImageCount() const {
    const views::View* root =
        GetContentsView()->GetViewByID(informed_restore::kOverflowViewID);
    CHECK(root);
    return GetViewCount(root, informed_restore::kOverflowImageViewID);
  }

  const InformedRestoreScreenshotIconRowView* GetScreenshotIconRowView() const {
    return static_cast<const InformedRestoreScreenshotIconRowView*>(
        GetContentsView()->GetViewByID(
            informed_restore::kScreenshotIconRowViewID));
  }

  // Used for testing overview. Returns a vector with `n` chrome browser app
  // ids.
  std::unique_ptr<InformedRestoreContentsData> MakeTestAppIds(int n) {
    auto data = std::make_unique<InformedRestoreContentsData>();

    // Create callbacks which close the informed restore overview session to
    // simulate production behavior.
    base::OnceClosure callback = base::BindLambdaForTesting([]() {
      Shell::Get()
          ->informed_restore_controller()
          ->MaybeEndInformedRestoreSession();
    });
    std::pair<base::OnceClosure, base::OnceClosure> split =
        base::SplitOnceCallback(std::move(callback));
    data->restore_callback = std::move(split.first);
    data->cancel_callback = std::move(split.second);

    for (int i = 0; i < n; ++i) {
      data->apps_infos.emplace_back(app_constants::kChromeAppId, "Title",
                                    /*window_id=*/i);
    }

    return data;
  }

  // Similar to above but adds a solid color image so that the screenshot UI
  // shows up.
  std::unique_ptr<InformedRestoreContentsData> MakeTestAppIdsWithImage(int n) {
    auto data = MakeTestAppIds(n);
    data->image = CreateSolidColorTestImage(gfx::Size(800, 600), SK_ColorRED);
    return data;
  }

  // TODO(minch): Make contents_data->image can be altered, for example,
  // some dummy image to make the test more unit-testy.
  // Takes a screenshot of the entire display and save it to the given
  // `file_path`, which is also set as the path to store the informed restore
  // screenshot.
  void TakeAndSaveInformedRestoreScreenshot(const base::FilePath& file_path) {
    SetInformedRestoreImagePathForTest(file_path);

    TakePrimaryDisplayScreenshotAndSave(file_path);
    int64_t file_size = 0;
    ASSERT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, 0);
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeTimeNow(base::Time fake_now) { fake_time_ = fake_now; }

  static base::TimeTicks FakeTimeTicksNow() { return fake_time_ticks_; }
  static void SetFakeTimeTicksNow(base::TimeTicks fake_now) {
    fake_time_ticks_ = fake_now;
  }

 private:
  InProcessDataDecoder decoder_;

  static inline base::Time fake_time_;
  static inline base::TimeTicks fake_time_ticks_;
};

TEST_F(InformedRestoreTest, NoOverflow) {
  // Start an informed restore overview session with restore data for one
  // window.
  StartOverviewSession(MakeTestAppIds(1));
  EXPECT_FALSE(
      GetContentsView()->GetViewByID(informed_restore::kOverflowViewID));
}

TEST_F(InformedRestoreTest, TwoWindowOverflow) {
  // Start an informed restore overview session with restore data for two
  // overflow windows.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kOverflowMinThreshold + 2));

  EXPECT_EQ(2u, GetOverflowImageCount());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x1 layout.
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowTopRowViewID)
                    ->children()
                    .size());
  EXPECT_FALSE(GetContentsView()->GetViewByID(
      informed_restore::kOverflowBottomRowViewID));
}

TEST_F(InformedRestoreTest, ThreeWindowOverflow) {
  // Start an informed restore overview session with restore data for three
  // overflow windows.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kOverflowMinThreshold + 3));

  EXPECT_EQ(3u, GetOverflowImageCount());

  // The top row should have one element, and the bottom row should have two
  // elements, in order to form a triangular layout.
  EXPECT_EQ(1u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowTopRowViewID)
                    ->children()
                    .size());
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowBottomRowViewID)
                    ->children()
                    .size());
}

TEST_F(InformedRestoreTest, FourWindowOverflow) {
  // Start an informed restore overview session with restore data for four
  // overflow windows.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kOverflowMinThreshold + 4));

  EXPECT_EQ(4u, GetOverflowImageCount());

  // The top and bottom rows should have two elements each, in order to form a
  // 2x2 layout.
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowTopRowViewID)
                    ->children()
                    .size());
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowBottomRowViewID)
                    ->children()
                    .size());
}

TEST_F(InformedRestoreTest, FivePlusWindowOverflow) {
  auto data = std::make_unique<InformedRestoreContentsData>();
  data->last_session_crashed = false;

  // Start an informed restore overview session with restore data for five
  // overflow windows.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kOverflowMinThreshold + 5));

  // The image view map should only have three elements as the fourth slot is
  // saved for a count of the remaining windows.
  EXPECT_EQ(3u, GetOverflowImageCount());

  // The top row should have two elements, and the bottom row should have zero
  // elements, in order to form a 2x2 layout.
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowTopRowViewID)
                    ->children()
                    .size());
  EXPECT_EQ(2u, GetContentsView()
                    ->GetViewByID(informed_restore::kOverflowBottomRowViewID)
                    ->children()
                    .size());
}

// Tests that the informed restore screenshot should not be shown if it has
// different orientation as the display will show it.
TEST_F(InformedRestoreTest, NoScreenshotWithDifferentDisplayOrientation) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  base::HistogramTester histogram_tester;
  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.png");
  TakeAndSaveInformedRestoreScreenshot(file_path);
  ASSERT_TRUE(base::PathExists(file_path));

  // Rotate the display and show the informed restore dialog.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  StartOverviewSession(MakeTestAppIds(1));
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  ASSERT_TRUE(contents_data);
  // The image inside `InformedRestoreContentsData` should be null when the
  // landscape image is going to be shown inside a display in the portrait
  // orientation.
  EXPECT_TRUE(contents_data->image.isNull());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
      testing::ElementsAre(base::Bucket(
          ScreenshotOnShutdownStatus::kFailedOnDifferentOrientations, 1)));
  // The previously saved image will not be shown this time and should be
  // deleted from the disk as well to avoid stale screenshot next time.
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(InformedRestoreTest, ScreenshotIconRowMaxElements) {
  base::HistogramTester histogram_tester;
  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.png");
  TakeAndSaveInformedRestoreScreenshot(file_path);
  ASSERT_TRUE(base::PathExists(file_path));

  // Starts the session with the maximum number of elements that can be shown
  // inside the icon row.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kScreenshotIconRowMaxElements));
  auto* controller = Shell::Get()->informed_restore_controller();
  const InformedRestoreContentsData* contents_data =
      controller->contents_data();
  EXPECT_TRUE(contents_data && !contents_data->image.isNull());
  // The image in the disk should be deleted to avoid stale screenshot on next
  // time to show the informed restore dialog.
  EXPECT_FALSE(base::PathExists(file_path));
  // Screenshot icon row should be shown when there is a screenshot.
  const InformedRestoreScreenshotIconRowView* screenshot_icon_row_view =
      GetScreenshotIconRowView();
  EXPECT_TRUE(screenshot_icon_row_view);
  // The icon row should show all the elements and all of them should be shown
  // as icons.
  EXPECT_EQ(5u, screenshot_icon_row_view->children().size());
  EXPECT_EQ(5u, GetViewCount(screenshot_icon_row_view,
                             informed_restore::kScreenshotImageViewID));
  histogram_tester.ExpectBucketCount(kDialogScreenshotVisibility, true, 1);
  histogram_tester.ExpectBucketCount(kDialogScreenshotVisibility, false, 0);

  controller->MaybeEndInformedRestoreSession();
  // Starts the session again, the dialog should show with listview instead of
  // the screenshot.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kScreenshotIconRowMaxElements));
  EXPECT_FALSE(GetScreenshotIconRowView());
}

TEST_F(InformedRestoreTest, ScreenshotIconRowExceedMaxElements) {
  base::ScopedTempDir temp_dir;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TakeAndSaveInformedRestoreScreenshot(
      temp_dir.GetPath().AppendASCII("test.png"));

  // Starts the session with more elements that can be shown inside the icon
  // row.
  StartOverviewSession(
      MakeTestAppIds(informed_restore::kScreenshotIconRowMaxElements + 2));
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  EXPECT_TRUE(contents_data && !contents_data->image.isNull());
  // Screenshot icon row should be shown when there is a screenshot.
  const InformedRestoreScreenshotIconRowView* screenshot_icon_row_view =
      GetScreenshotIconRowView();
  EXPECT_TRUE(screenshot_icon_row_view);
  // The icon row should still have at most 5 number of items, but only 4 of
  // them should be icons. The last one should be a count label.
  EXPECT_EQ(5u, screenshot_icon_row_view->children().size());
  EXPECT_EQ(4u, GetViewCount(screenshot_icon_row_view,
                             informed_restore::kScreenshotImageViewID));
}

// Tests that based on preferences (shown count, and last shown time), the nudge
// may or may not be shown.
TEST_F(InformedRestoreTest, NudgePreferences) {
  SetFakeTimeNow(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides time_override(
      &InformedRestoreTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto* anchored_nudge_manager = Shell::Get()->anchored_nudge_manager();

  auto test_start_and_end_overview = [&]() {
    // Reset the nudge if it's currently showing.
    anchored_nudge_manager->Cancel(informed_restore::kSuggestionsNudgeId);
    StartOverviewSession(MakeTestAppIds(1));
    ToggleOverview();
  };

  // Start inforemd restore session, then end overview. Test we show the nudge.
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));

  // Start and end overview. This does not show the nudge as 24 hours have not
  // elapsed since the nudge was shown.
  test_start_and_end_overview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));
  // Start and end overview after waiting 25 hours. The nudge should now show
  // for the second time.
  SetFakeTimeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));

  // Show the nudge for a third time. This will be the last time it is shown.
  SetFakeTimeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));

  // Advance the clock and attempt to show the nudge for a fourth time. Verify
  // that it will not show.
  SetFakeTimeNow(FakeTimeNow() + base::Hours(25));
  test_start_and_end_overview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));
}

// Tests that we only show the nudge for informed restore overview.
TEST_F(InformedRestoreTest, InformedRestoreNudge) {
  StartOverviewSession(MakeTestAppIds(1));

  ToggleOverview();
  auto* anchored_nudge_manager = Shell::Get()->anchored_nudge_manager();
  EXPECT_TRUE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));
  anchored_nudge_manager->Cancel(informed_restore::kSuggestionsNudgeId);

  // Reset `contents_data` so we start normal overview.
  InformedRestoreTestApi().SetInformedRestoreContentsDataForTesting(nullptr);

  // Start and end overview normally. Test we don't show the nudge.
  ToggleOverview();
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(OverviewGridTestApi(overview_grid).informed_restore_widget());
  ToggleOverview();
  EXPECT_FALSE(anchored_nudge_manager->GetShownNudgeForTest(
      informed_restore::kSuggestionsNudgeId));
}

// Tests that the onboarding dialog toast shows up when clicking the accept
// button for the onboarding dialog, if restore was previously off.
TEST_F(InformedRestoreTest, OnboardingToast) {
  GetTestPrefService()->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);

  EXPECT_FALSE(
      ToastManager::Get()->IsToastShown(informed_restore::kOnboardingToastId));
  Shell::Get()
      ->informed_restore_controller()
      ->MaybeShowInformedRestoreOnboarding(
          /*restore_on=*/false);
  auto* dialog = InformedRestoreTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetAcceptButtonForTesting());
  EXPECT_TRUE(
      ToastManager::Get()->IsToastShown(informed_restore::kOnboardingToastId));
}

// Tests the onboarding metrics are recorded correctly.
TEST_F(InformedRestoreTest, OnboardingMetrics) {
  base::HistogramTester histogram_tester;

  // The pref is set to false in tests by default.
  GetTestPrefService()->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);

  // Verify initial histogram counts.
  histogram_tester.ExpectTotalCount(kInformedRestoreOnboardingHistogram, 0);

  // Press "Accept". Test we increment `true`.
  auto* controller = Shell::Get()->informed_restore_controller();
  controller->MaybeShowInformedRestoreOnboarding(/*restore_on=*/false);
  auto* dialog = InformedRestoreTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetAcceptButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  histogram_tester.ExpectBucketCount(kInformedRestoreOnboardingHistogram,
                                     /*sample=*/true,
                                     /*expected_count=*/1);
  GetTestPrefService()->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);

  // Press "Cancel". Test we increment `false`.
  controller->MaybeShowInformedRestoreOnboarding(/*restore_on=*/false);
  dialog = InformedRestoreTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetCancelButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  histogram_tester.ExpectBucketCount(kInformedRestoreOnboardingHistogram,
                                     /*sample=*/false,
                                     /*expected_count=*/1);
  GetTestPrefService()->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);

  // Verify total counts.
  histogram_tester.ExpectTotalCount(kInformedRestoreOnboardingHistogram, 2);

  // Show the onboarding dialog with 'Restore' on. Test we don't record.
  controller->MaybeShowInformedRestoreOnboarding(
      /*restore_on=*/true);
  dialog = InformedRestoreTestApi().GetOnboardingDialog();
  LeftClickOn(dialog->GetAcceptButtonForTesting());
  views::test::WidgetDestroyedWaiter(dialog->GetWidget()).Wait();
  histogram_tester.ExpectTotalCount(kInformedRestoreOnboardingHistogram, 2);
}

// Tests that the metric which records the time between when the user clicks
// restore or cancel from when the dialog is created is logged properly.
TEST_F(InformedRestoreTest, TimeToActionMetrics) {
  SetFakeTimeTicksNow(base::TimeTicks::Now());
  base::subtle::ScopedTimeClockOverrides time_override(
      /*time_override=*/nullptr, &InformedRestoreTest::FakeTimeTicksNow,
      /*thread_ticks_override=*/nullptr);

  base::HistogramTester histogram_tester;
  StartOverviewSession(MakeTestAppIds(1));

  // Click the restore button after one second.
  SetFakeTimeTicksNow(base::TimeTicks::Now() + base::Seconds(1));
  const views::View* restore_button =
      GetContentsView()->GetViewByID(informed_restore::kRestoreButtonID);
  LeftClickOn(restore_button);

  // The buckets are split into bucket by time deltas, so we check the size of
  // all samples.
  EXPECT_EQ(
      1u,
      histogram_tester.GetAllSamples("Ash.Pine.TimeToAction.Listview").size());

  StartOverviewSession(MakeTestAppIds(1));

  // Click the cancel button after 2 seconds.
  SetFakeTimeTicksNow(base::TimeTicks::Now() + base::Seconds(2));
  const views::View* cancel_button =
      GetContentsView()->GetViewByID(informed_restore::kCancelButtonID);
  LeftClickOn(cancel_button);
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples("Ash.Pine.TimeToAction.Listview").size());
}

TEST_F(InformedRestoreTest, CloseDialogMetrics) {
  base::HistogramTester histogram_tester_list_view;

  // Test clicking the restore button.
  StartOverviewSession(MakeTestAppIds(1));
  const views::View* restore_button1 =
      GetContentsView()->GetViewByID(informed_restore::kRestoreButtonID);
  LeftClickOn(restore_button1);
  EXPECT_THAT(
      histogram_tester_list_view.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(base::Bucket(
          base::to_underlying(CloseDialogType::kListviewRestoreButton), 1)));

  // Test clicking the cancel button.
  StartOverviewSession(MakeTestAppIds(1));
  const views::View* cancel_button1 =
      GetContentsView()->GetViewByID(informed_restore::kCancelButtonID);
  LeftClickOn(cancel_button1);
  EXPECT_THAT(
      histogram_tester_list_view.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(
          base::Bucket(
              base::to_underlying(CloseDialogType::kListviewRestoreButton), 1),
          base::Bucket(
              base::to_underlying(CloseDialogType::kListviewCancelButton), 1)));

  // Test exiting the informed restore overview session without clicking any
  // buttons.
  StartOverviewSession(MakeTestAppIds(1));
  Shell::Get()->informed_restore_controller()->MaybeEndInformedRestoreSession();
  EXPECT_THAT(
      histogram_tester_list_view.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(
          base::Bucket(
              base::to_underlying(CloseDialogType::kListviewRestoreButton), 1),
          base::Bucket(
              base::to_underlying(CloseDialogType::kListviewCancelButton), 1),
          base::Bucket(base::to_underlying(CloseDialogType::kListviewOther),
                       1)));

  // Run the same tests but with the screenshot UI.
  base::HistogramTester histogram_tester_screenshot;

  StartOverviewSession(MakeTestAppIdsWithImage(1));
  const views::View* restore_button2 =
      GetContentsView()->GetViewByID(informed_restore::kRestoreButtonID);
  LeftClickOn(restore_button2);
  EXPECT_THAT(
      histogram_tester_screenshot.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(base::Bucket(
          base::to_underlying(CloseDialogType::kScreenshotRestoreButton), 1)));

  StartOverviewSession(MakeTestAppIdsWithImage(1));
  const views::View* cancel_button2 =
      GetContentsView()->GetViewByID(informed_restore::kCancelButtonID);
  LeftClickOn(cancel_button2);
  EXPECT_THAT(
      histogram_tester_screenshot.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(base::Bucket(base::to_underlying(
                                  CloseDialogType::kScreenshotRestoreButton),
                              1),
                 base::Bucket(base::to_underlying(
                                  CloseDialogType::kScreenshotCancelButton),
                              1)));

  StartOverviewSession(MakeTestAppIdsWithImage(1));
  Shell::Get()->informed_restore_controller()->MaybeEndInformedRestoreSession();
  EXPECT_THAT(
      histogram_tester_screenshot.GetAllSamples(kDialogClosedHistogram),
      BucketsAre(
          base::Bucket(
              base::to_underlying(CloseDialogType::kScreenshotRestoreButton),
              1),
          base::Bucket(
              base::to_underlying(CloseDialogType::kScreenshotCancelButton), 1),
          base::Bucket(base::to_underlying(CloseDialogType::kScreenshotOther),
                       1)));
}

// Tests that if we exit overview without clicking the restore or cancel
// buttons, the informed restore widget gets shown when entering overview next.
TEST_F(InformedRestoreTest, ToggleOverviewToExit) {
  StartOverviewSession(MakeTestAppIds(1));

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_TRUE(OverviewGridTestApi(overview_grid).informed_restore_widget());

  // Exit overview by without clicking the restore or cancel buttons.
  ToggleOverview();
  ASSERT_FALSE(OverviewController::Get()->overview_session());

  ToggleOverview();
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_TRUE(OverviewGridTestApi(overview_grid).informed_restore_widget());
}

TEST_F(InformedRestoreTest, ClickRestoreToExit) {
  StartOverviewSession(MakeTestAppIds(1));

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  views::Widget* widget =
      OverviewGridTestApi(overview_grid).informed_restore_widget();
  ASSERT_TRUE(widget);

  // Exit overview by clicking the restore or cancel buttons.
  const views::View* restore_button = widget->GetContentsView()->GetViewByID(
      informed_restore::kRestoreButtonID);
  LeftClickOn(restore_button);
  ASSERT_FALSE(OverviewController::Get()->overview_session());

  ToggleOverview();
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_FALSE(OverviewGridTestApi(overview_grid).informed_restore_widget());
}

TEST_F(InformedRestoreTest, InformedRestoreItemView) {
  InformedRestoreContentsData::AppInfo app_info(
      "TEST_ID", "TEST_TITLE", /*window_id=*/0,
      std::vector<GURL>{GURL(), GURL(), GURL(), GURL()}, 4u, 0);

  // Test when the tab count is within regular limits.
  auto item_view = std::make_unique<InformedRestoreItemView>(
      app_info, /*inside_screenshot=*/false);
  EXPECT_EQ(4u,
            item_view->GetViewByID(informed_restore::kFaviconContainerViewID)
                ->children()
                .size());
  item_view.reset();

  // Test the when the tab count has overflow.
  app_info.tab_count = 10u;
  item_view = std::make_unique<InformedRestoreItemView>(
      app_info, /*inside_screenshot=*/false);
  EXPECT_EQ(5u,
            item_view->GetViewByID(informed_restore::kFaviconContainerViewID)
                ->children()
                .size());
}

// Tests that the informed restore dialog remains in the center after zooming
// the display up or down.
TEST_F(InformedRestoreTest, ZoomDisplay) {
  StartOverviewSession(MakeTestAppIds(1));

  aura::Window* root = Shell::GetPrimaryRootWindow();
  OverviewGrid* overview_grid = GetOverviewGridForRoot(root);
  ASSERT_TRUE(overview_grid);
  OverviewGridTestApi overview_grid_test_api(overview_grid);
  views::Widget* widget = overview_grid_test_api.informed_restore_widget();
  ASSERT_TRUE(widget);
  const gfx::Rect& initial_bounds = widget->GetWindowBoundsInScreen();
  const int half_birch_bar_paddings =
      16 + 0.5 * overview_grid_test_api.birch_bar_widget()
                     ->GetWindowBoundsInScreen()
                     .height();

  // Checks the widget bounds. The x should be exactly centered in the display,
  // the y is near the place half of birch bar paddings away from center and the
  // size remains the same.
  auto verify_widget_bounds = [&root, &widget, &initial_bounds,
                               &half_birch_bar_paddings](
                                  const std::string& test_name) {
    SCOPED_TRACE(test_name);
    const gfx::Rect root_bounds = root->GetBoundsInScreen();
    const gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
    EXPECT_EQ(root_bounds.CenterPoint().x(), widget_bounds.CenterPoint().x());
    EXPECT_LT(widget_bounds.CenterPoint().y(), root_bounds.CenterPoint().y());
    EXPECT_GT(widget_bounds.CenterPoint().y(),
              root_bounds.CenterPoint().y() - 20 - half_birch_bar_paddings);
    EXPECT_EQ(initial_bounds.size(), widget_bounds.size());
  };

  // Zoom up twice and down once and verify the bounds of the informed restore
  // widget at all stages.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int64_t display_id = WindowTreeHostManager::GetPrimaryDisplayId();
  display_manager->ZoomDisplay(display_id, /*up=*/true);
  verify_widget_bounds("Zoom 1, up");
  display_manager->ZoomDisplay(display_id, /*up=*/true);
  verify_widget_bounds("Zoom 2, up");
  display_manager->ZoomDisplay(display_id, /*up=*/false);
  verify_widget_bounds("Zoom 2, down");
}

TEST_F(InformedRestoreTest, InformedRestoreWidgetTabTraversal) {
  StartOverviewSession(MakeTestAppIds(1));

  views::Widget* widget =
      OverviewGridTestApi(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow()))
          .informed_restore_widget();
  ASSERT_TRUE(widget);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  const InformedRestoreContentsView* contents = GetContentsView();

  // Tab a couple times through the informed restore widgets focusable views.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(contents->GetViewByID(informed_restore::kCancelButtonID),
            focus_manager->GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(contents->GetViewByID(informed_restore::kRestoreButtonID),
            focus_manager->GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(contents->GetViewByID(informed_restore::kSettingsButtonID),
            focus_manager->GetFocusedView());

  // The focus is now on a view not associated with `focus_manager`.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(focus_manager->GetFocusedView());

  // Reverse focus and verify it lands on the informed restore widgets last
  // focusable view.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(contents->GetViewByID(informed_restore::kSettingsButtonID),
            focus_manager->GetFocusedView());
}

// Tests that there is no desk bar in an informed restore overview session.
TEST_F(InformedRestoreTest, NoDeskBar) {
  StartOverviewSession(MakeTestAppIds(1));

  // There should be no desk bar.
  EXPECT_FALSE(
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_widget());

  // Exit and reenter overview mode. There should still be no desk bar.
  ToggleOverview();
  ToggleOverview();
  EXPECT_FALSE(
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_widget());
}

// Tests that the contents are laid out correctly when the display is in
// landscape mode and transitions to portrait mode.
TEST_F(InformedRestoreTest, LayoutLandscapeToPortrait) {
  StartOverviewSession(MakeTestAppIds(1));

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  views::Widget* widget =
      OverviewGridTestApi(overview_grid).informed_restore_widget();
  ASSERT_TRUE(widget);

  // In landscape mode, the `InformedRestoreContentsView` should have two
  // children: a left hand side contents view, and a right hand side contents
  // view.
  auto* contents_view = views::AsViewClass<InformedRestoreContentsView>(
      widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(contents_view->children().size(), 2u);

  // Rotate the display to put it in portrait mode.
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  ASSERT_TRUE(chromeos::IsPortraitOrientation(
      orientation_test_api.GetCurrentOrientation()));
  ASSERT_TRUE(widget);

  // In portrait mode, the `InformedRestoreContentsView` should have three
  // children: the title and description container (header), the
  // `InformedRestoreItemsContainerView`, and the buttons container
  // (footer).
  contents_view = views::AsViewClass<InformedRestoreContentsView>(
      widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(contents_view->children().size(), 3u);
}

// Tests that the contents are laid out correctly when the display is in
// portrait mode and transitions to landscape mode.
TEST_F(InformedRestoreTest, LayoutPortraitToLandscape) {
  // Rotate the display to put it in portrait mode.
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  ASSERT_TRUE(chromeos::IsPortraitOrientation(
      orientation_test_api.GetCurrentOrientation()));

  StartOverviewSession(MakeTestAppIds(1));

  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  views::Widget* widget =
      OverviewGridTestApi(overview_grid).informed_restore_widget();
  ASSERT_TRUE(widget);

  // In portrait mode, the `InformedRestoreContentsView` should have three
  // children: the title and description container (header), the
  // `InformedRestoreItemsContainerView`, and the buttons container
  // (footer).
  auto* contents_view = views::AsViewClass<InformedRestoreContentsView>(
      widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(contents_view->children().size(), 3u);

  // Rotate the display again to put it in landscape mode.
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_180, display::Display::RotationSource::ACTIVE);
  ASSERT_FALSE(chromeos::IsPortraitOrientation(
      orientation_test_api.GetCurrentOrientation()));
  ASSERT_TRUE(widget);

  // In landscape mode, the `InformedRestoreContentsView` should have two
  // children: a left hand side contents view, and a right hand side contents
  // view.
  contents_view = views::AsViewClass<InformedRestoreContentsView>(
      widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(contents_view->children().size(), 2u);
}

class InformedRestoreAppIconTest : public InformedRestoreTest {
 public:
  void SetUp() override {
    InformedRestoreTest::SetUp();

    AccountId account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();
    registry_cache_.SetAccountId(account_id);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id,
                                                             &registry_cache_);
  }

  void TearDown() override {
    InformedRestoreTest::TearDown();

    apps::AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(
        &registry_cache_);
    registry_cache_.ReinitializeForTesting();
  }

  TestSavedDeskDelegate* GetTestSavedDeskDelegate() {
    return static_cast<TestSavedDeskDelegate*>(
        Shell::Get()->saved_desk_delegate());
  }

 protected:
  apps::AppRegistryCache registry_cache_;
};

// Tests that `InformedRestoreAppImageView` properly updates the displayed image
// when the app with the given ID is installed.
TEST_F(InformedRestoreAppIconTest, UpdateAfterSessionStarted) {
  // The intended icon for our test app. It should not be shown until the app is
  // "updated".
  gfx::ImageSkia test_icon =
      CreateSolidColorTestImage(gfx::Size(1, 1), SK_ColorRED);
  const std::string test_id = "TEST_ID";

  auto data = std::make_unique<InformedRestoreContentsData>();
  data->apps_infos.emplace_back(test_id, "TEST_TITLE", /*window_id=*/0);
  StartOverviewSession(std::move(data));

  // Before installation, the image view should show the default app icon, and
  // the title should be empty.
  const InformedRestoreAppImageView* image_view =
      views::AsViewClass<InformedRestoreAppImageView>(
          GetContentsView()->GetViewByID(informed_restore::kItemImageViewID));
  ASSERT_TRUE(image_view);
  EXPECT_FALSE(image_view->GetImage().isNull());
  EXPECT_FALSE(gfx::test::AreImagesClose(gfx::Image(image_view->GetImage()),
                                         gfx::Image(test_icon),
                                         /*max_deviation=*/0));

  const InformedRestoreItemView* item_view =
      views::AsViewClass<InformedRestoreItemView>(
          GetContentsView()->GetViewByID(informed_restore::kItemViewID));
  ASSERT_TRUE(item_view);
  ASSERT_TRUE(item_view->title_label_view());
  EXPECT_EQ(u"TEST_TITLE", item_view->title_label_view()->GetText());

  // Update the test delegate to return a valid icon the next time one is
  // requested.
  GetTestSavedDeskDelegate()->set_default_app_icon(test_icon);

  // Using the existing app ID, mark the app as ready, so `app_image_view` will
  // update with the new image. Also provide a new title for the app.
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, test_id);
  app->readiness = apps::Readiness::kReady;
  app->name = "UPDATED_TITLE";
  std::vector<apps::AppPtr> registry_deltas;
  registry_deltas.push_back(std::move(app));
  registry_cache_.OnAppsForTesting(std::move(registry_deltas),
                                   apps::AppType::kWeb,
                                   /*should_notify_initialized=*/false);

  // The image and title should now be valid.
  EXPECT_FALSE(image_view->GetImage().isNull());
  EXPECT_TRUE(gfx::test::AreImagesClose(gfx::Image(image_view->GetImage()),
                                        gfx::Image(test_icon),
                                        /*max_deviation=*/0));
  EXPECT_EQ(u"UPDATED_TITLE", item_view->title_label_view()->GetText());
}

}  // namespace ash
