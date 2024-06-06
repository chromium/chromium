// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

namespace {

constexpr int kBrowserAppIndexOnShelf = 0;

// A test shelf item delegate that simulates an activated window when a shelf
// item is selected. When |wait_for_tablet_mode_| is set, the delegate will wait
// for tablet mode animation start to run the callback that activates the
// window.
class TestShelfItemDelegate : public ShelfItemDelegate,
                              display::DisplayObserver {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  TestShelfItemDelegate(const ShelfID& shelf_id, bool wait_for_tablet_mode)
      : ShelfItemDelegate(shelf_id),
        wait_for_tablet_mode_(wait_for_tablet_mode) {
    if (wait_for_tablet_mode_) {
      display::Screen::GetScreen()->AddObserver(this);
    }
  }

  ~TestShelfItemDelegate() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    if (wait_for_tablet_mode_) {
      callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}

  void OnDisplayTabletStateChanged(display::TabletState state) override {
    if (!callback_ || state != display::TabletState::kEnteringTabletMode) {
      return;
    }
    std::move(callback_).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }

 private:
  bool wait_for_tablet_mode_ = false;
  ItemSelectedCallback callback_;
};

}  // namespace

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

// Used to test that app launched metrics are properly recorded.
class AppListMetricsTest : public AshTestBase {
 public:
  AppListMetricsTest() = default;

  AppListMetricsTest(const AppListMetricsTest&) = delete;
  AppListMetricsTest& operator=(const AppListMetricsTest&) = delete;

  ~AppListMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    search_model_ = AppListModelProvider::Get()->search_model();

    shelf_test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
  }

 protected:
  void CreateShelfItem(bool wait_for_tablet_mode = false) {
    // Add shelf item to be launched. Waits for the shelf view's bounds
    // animations to end.
    ShelfItem shelf_item;
    shelf_item.id = ShelfID("app_id");
    shelf_item.type = TYPE_BROWSER_SHORTCUT;
    ShelfModel::Get()->Add(shelf_item,
                           std::make_unique<TestShelfItemDelegate>(
                               shelf_item.id, wait_for_tablet_mode));
    shelf_test_api_->RunMessageLoopUntilAnimationsDone();
  }

  void CreateAndClickShelfItem() {
    CreateShelfItem();
    ClickShelfItem();
  }

  void ClickShelfItem() {
    // Get location of the shelf item.
    const views::ViewModel* view_model =
        GetPrimaryShelf()->GetShelfViewForTesting()->view_model_for_test();
    LeftClickOn(view_model->view_at(kBrowserAppIndexOnShelf));
  }

  void PopulateAndLaunchAppInGrid(int num = 4) {
    // Populate apps in the root app grid.
    AppListModel* model = AppListModelProvider::Get()->model();
    for (int i = 0; i < num; i++) {
      AppListItem* item = model->AddItem(
          std::make_unique<AppListItem>(base::StringPrintf("item %d", i)));
      // Give each item a name so that the accessibility paint checks pass.
      // (Focusable items should have accessible names.)
      model->SetItemName(item, item->id());
    }

    AppListView::TestApi test_api(
        Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView());

    // Focus the first item in the root app grid.
    test_api.GetRootAppsGridView()->GetItemViewAt(0)->RequestFocus();

    // Press return to simulate an app launch from a grid item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }

 private:
  raw_ptr<SearchModel, DanglingUntriaged> search_model_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> shelf_test_api_;
};

// Suite for tests that run in tablet mode.
class AppListMetricsTabletTest : public AppListMetricsTest {
 public:
  AppListMetricsTabletTest() = default;
  ~AppListMetricsTabletTest() override = default;
  void SetUp() override {
    AppListMetricsTest::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }
};

// Test that the histogram records an app launch from the shelf while the
// homecher all apps state is showing.
TEST_F(AppListMetricsTabletTest, HomecherAllAppsLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the shelf while the
// the tablet animation is running.
TEST_F(AppListMetricsTest, TapOnItemDuringTabletModeAnimation) {
  base::HistogramTester histogram_tester;
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  CreateShelfItem(/*wait_for_tablet_mode=*/true);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ClickShelfItem();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetPrimaryShelf()->shelf_widget()->LayoutRootViewIfNecessary();
  GetAppListTestHelper()->CheckVisibility(false);

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Closed",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the app grid while the
// homecher all apps state is showing.
TEST_F(AppListMetricsTabletTest, HomecherAllAppsLaunchFromGrid) {
  base::HistogramTester histogram_tester;
  PopulateAndLaunchAppInGrid();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromGrid,
      1 /* Number of times launched from grid */);
}

// Test that the histogram records an app launch from the shelf while the
// homecher search state is showing.
TEST_F(AppListMetricsTabletTest, HomecherSearchLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherSearch",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from a recent app suggestion
// while the bubble launcher all apps is showing.
TEST_F(AppListMetricsTest, BubbleAllAppsLaunchFromRecentApps) {
  base::HistogramTester histogram_tester;
  auto* helper = GetAppListTestHelper();

  helper->WaitUntilIdle();

  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  helper->ShowAppList();

  helper->WaitUntilIdle();
  views::View* recent_apps = helper->GetBubbleRecentAppsView();

  // Get focus on the first chip.
  recent_apps->children().front()->RequestFocus();
  helper->WaitUntilIdle();

  // Press return to simulate an app launch from the recent apps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  helper->WaitUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.BubbleAllApps",
      AppListLaunchedFrom::kLaunchedFromRecentApps,
      1 /* Number of times launched from chip */);
}

TEST_F(AppListMetricsTest, HideContinueSectionMetricInClamshellMode) {
  base::HistogramTester histograms;

  // Show the app list with a full continue section.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Metric is recorded in false bucket.
  const int false_bucket = 0;
  histograms.ExpectBucketCount(
      "Apps.AppList.ContinueSectionHiddenByUser.ClamshellMode", false_bucket,
      1);
  helper->Dismiss();

  // Hide the continue section, then show the app list.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);
  helper->ShowAppList();

  // Metric is recorded in true bucket.
  const int true_bucket = 1;
  histograms.ExpectBucketCount(
      "Apps.AppList.ContinueSectionHiddenByUser.ClamshellMode", true_bucket, 1);
  helper->Dismiss();
}

TEST_F(AppListMetricsTest, HideContinueSectionMetricInTabletMode) {
  base::HistogramTester histograms;

  // Show the tablet mode app list with a full continue section.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Metric is recorded in false bucket.
  const int false_bucket = 0;
  histograms.ExpectBucketCount(
      "Apps.AppList.ContinueSectionHiddenByUser.TabletMode", false_bucket, 1);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // Hide the continue section, then show the tablet mode app list.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Metric is recorded in true bucket.
  const int true_bucket = 1;
  histograms.ExpectBucketCount(
      "Apps.AppList.ContinueSectionHiddenByUser.TabletMode", true_bucket, 1);
}

// Test that the histogram records an app launch from a recent app suggestion
// while the homecher all apps is showing.
TEST_F(AppListMetricsTest, HomecherLaunchFromRecentApps) {
  base::HistogramTester histogram_tester;
  auto* helper = GetAppListTestHelper();

  helper->WaitUntilIdle();

  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  helper->WaitUntilIdle();
  views::View* recent_apps = helper->GetFullscreenRecentAppsView();

  // Get focus on the first chip.
  recent_apps->children().front()->RequestFocus();
  helper->WaitUntilIdle();

  // Press return to simulate an app launch from the recent apps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  helper->WaitUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromRecentApps,
      1 /* Number of times launched from chip */);
}

class AppListShowSourceMetricTest : public AshTestBase {
 public:
  AppListShowSourceMetricTest() = default;

  AppListShowSourceMetricTest(const AppListShowSourceMetricTest&) = delete;
  AppListShowSourceMetricTest& operator=(const AppListShowSourceMetricTest&) =
      delete;

  ~AppListShowSourceMetricTest() override = default;

 protected:
  void ClickHomeButton() {
    LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  }
};

// In tablet mode, test that AppListShowSource metric is only recorded when
// pressing home button when not already home. Any presses on the home button
// when already home should do nothing.
TEST_F(AppListShowSourceMetricTest, TabletInAppToHome) {
  base::HistogramTester histogram_tester;

  // Enable accessibility feature that forces home button to be shown in tablet
  // mode.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ClickHomeButton();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", AppListShowSource::kShelfButton,
      1 /* Number of times app list is shown with a shelf button */);
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", AppListShowSource::kTabletMode,
      0 /* Number of times app list is shown by tablet mode transition */);

  GetAppListTestHelper()->CheckVisibility(true);

  // Ensure that any subsequent clicks while already at home do not count as
  // showing the app list.
  ClickHomeButton();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", AppListShowSource::kShelfButton,
      1 /* Number of times app list shown with a shelf button */);
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 1);
}

// Ensure that app list is not recorded as shown when going to tablet mode with
// a window open.
TEST_F(AppListShowSourceMetricTest, TabletModeWithWindowOpen) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckVisibility(false);

  // Ensure that no AppListShowSource metric was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 0);
}

// Ensure that app list is recorded as shown when going to tablet mode with no
// other windows open.
TEST_F(AppListShowSourceMetricTest, TabletModeWithNoWindowOpen) {
  base::HistogramTester histogram_tester;

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckVisibility(true);

  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", AppListShowSource::kTabletMode,
      1 /* Number of times app list shown after entering tablet mode */);
}

// Tests that showing the bubble launcher in clamshell mode records the proper
// metrics for Apps.AppListBubbleShowSource.
TEST_F(AppListShowSourceMetricTest, ClamshellModeHomeButton) {
  base::HistogramTester histogram_tester;
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  // Show the Bubble AppList.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_bubble_presenter->IsShowing());

  // Test that the proper histogram is logged.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);

  // Hide the Bubble AppList.
  GetAppListTestHelper()->Dismiss();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Test that no histograms were logged.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);

  // Show the Bubble AppList one more time.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_bubble_presenter->IsShowing());

  // Test that the histogram records 2 total shows.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 2);

  // Test that no fullscreen app list metrics were recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 0);
}

// Test that tablet mode launcher operations do not record AppListBubble
// metrics.
TEST_F(AppListShowSourceMetricTest, TabletModeDoesNotRecordAppListBubbleShow) {
  base::HistogramTester histogram_tester;
  // Enable accessibility feature that forces home button to be shown in tablet
  // mode.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);

  // Go to tablet mode, the tablet mode (non bubble) launcher will show. Create
  // a test widget so the launcher will show in the background.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Ensure that no AppListBubbleShowSource metric was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 0);

  // Press the Home Button, which hides `widget` and shows the tablet mode
  // launcher.
  ClickHomeButton();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Test that no bubble launcher metrics were recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 0);
}

// Tests that toggling the bubble launcher does not record metrics when the
// result of the toggle is that the launcher is hidden.
TEST_F(AppListShowSourceMetricTest, ToggleDoesNotRecordOnHide) {
  base::HistogramTester histogram_tester;
  auto* app_list_controller = Shell::Get()->app_list_controller();

  // Toggle the app list to show it.
  app_list_controller->ToggleAppList(GetPrimaryDisplayId(),
                                     AppListShowSource::kSearchKey,
                                     base::TimeTicks::Now());
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  ASSERT_TRUE(app_list_bubble_presenter->IsShowing());

  // Toggle the app list once more, to hide it.
  app_list_controller->ToggleAppList(GetPrimaryDisplayId(),
                                     AppListShowSource::kSearchKey,
                                     base::TimeTicks::Now());
  ASSERT_FALSE(app_list_bubble_presenter->IsShowing());
  // Test that only one show was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);
}

using AppListPeriodicMetricsTest = AshTestBase;

// Verify that the number of items in the app list are recorded correctly.
TEST_F(AppListPeriodicMetricsTest, PeriodicAppListMetrics_NumberOfApps) {
  base::HistogramTester histogram;
  histogram.ExpectTotalCount("Apps.AppList.NumberOfApps", 0);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfRootLevelItems", 0);

  AppListModel* model = AppListModelProvider::Get()->model();

  // Add 5 items to the app list.
  for (int i = 0; i < 5; i++) {
    model->AddItem(
        std::make_unique<AppListItem>(base::StringPrintf("app_id_%d", i)));
  }

  // Check that 5 items are recorded as being in the app list.
  RecordPeriodicAppListMetrics();
  histogram.ExpectBucketCount("Apps.AppList.NumberOfApps", 5, 1);
  histogram.ExpectBucketCount("Apps.AppList.NumberOfRootLevelItems", 5, 1);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfApps", 1);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfRootLevelItems", 1);

  // Create a folder and add 3 items to it.
  const std::string folder_id = "folder_id";
  model->CreateFolderItem(folder_id);
  for (int i = 0; i < 3; i++) {
    auto item =
        std::make_unique<AppListItem>(base::StringPrintf("id_in_folder_%d", i));
    model->AddItemToFolder(std::move(item), folder_id);
  }

  // Check that the folder and its items are recorded in the metrics.
  RecordPeriodicAppListMetrics();
  histogram.ExpectBucketCount("Apps.AppList.NumberOfApps", 8, 1);
  histogram.ExpectBucketCount("Apps.AppList.NumberOfRootLevelItems", 6, 1);
}

TEST_F(AppListPeriodicMetricsTest, RecordFolderMetrics_ZeroFolders) {
  base::HistogramTester histogram;
  GetAppListTestHelper()->model()->PopulateApps(2);

  RecordPeriodicAppListMetrics();

  // 1 sample in the 0 folders bucket.
  EXPECT_EQ(1, histogram.GetBucketCount("Apps.AppList.NumberOfFolders", 0));
  // 1 sample in the 0 folders bucket.
  EXPECT_EQ(
      1, histogram.GetBucketCount("Apps.AppList.NumberOfNonSystemFolders", 0));
  // 1 sample in the 0 apps bucket.
  EXPECT_EQ(1, histogram.GetBucketCount(
                   "Apps.AppList.NumberOfAppsInNonSystemFolders", 0));
}

TEST_F(AppListPeriodicMetricsTest, RecordFolderMetrics_OneRegularFolder) {
  base::HistogramTester histogram;
  GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(2);

  RecordPeriodicAppListMetrics();

  // 1 sample in the 1 folder bucket.
  EXPECT_EQ(1, histogram.GetBucketCount("Apps.AppList.NumberOfFolders", 1));
  // 1 sample in the 1 folder bucket.
  EXPECT_EQ(
      1, histogram.GetBucketCount("Apps.AppList.NumberOfNonSystemFolders", 1));
  // 1 sample in the 2 apps bucket.
  EXPECT_EQ(1, histogram.GetBucketCount(
                   "Apps.AppList.NumberOfAppsInNonSystemFolders", 2));
}

TEST_F(AppListPeriodicMetricsTest, RecordFolderMetrics_SystemFolder) {
  base::HistogramTester histogram;
  AppListFolderItem* folder =
      GetAppListTestHelper()->model()->CreateSingleItemFolder("folder_id",
                                                              "item_id");
  folder->SetIsSystemFolder(true);

  RecordPeriodicAppListMetrics();

  // 1 sample in the 1 folder bucket.
  EXPECT_EQ(1, histogram.GetBucketCount("Apps.AppList.NumberOfFolders", 1));
  // 1 sample in the 0 folders bucket, because the folder is a system folder.
  EXPECT_EQ(
      1, histogram.GetBucketCount("Apps.AppList.NumberOfNonSystemFolders", 0));
  // 1 sample in the 0 apps bucket, because items in system folders don't count.
  EXPECT_EQ(1, histogram.GetBucketCount(
                   "Apps.AppList.NumberOfAppsInNonSystemFolders", 0));
}

}  // namespace ash
