// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/remove_query_confirmation_dialog.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kBestMatchContainerIndex = 1;

SearchModel* GetSearchModel() {
  return AppListModelProvider::Get()->search_model();
}

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void EnableTabletMode(bool enable) {
  // Avoid |TabletModeController::OnGetSwitchStates| from disabling tablet mode
  // again at the end of |TabletModeController::TabletModeController|.
  base::RunLoop().RunUntilIdle();

  if (enable) {
    ash::TabletModeControllerTestApi().EnterTabletMode();
  } else {
    ash::TabletModeControllerTestApi().LeaveTabletMode();
  }

  // The app list will be shown automatically when tablet mode is enabled (Home
  // launcher flag is enabled). Wait here for the animation complete.
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<TestSearchResult> CreateOmniboxSuggestionResult(
    const std::string& result_id,
    bool support_removal) {
  auto suggestion_result = std::make_unique<TestSearchResult>();
  suggestion_result->set_result_id(result_id);
  suggestion_result->set_best_match(true);
  suggestion_result->set_display_type(SearchResultDisplayType::kList);
  if (support_removal) {
    SearchResultActions actions;
    actions.emplace_back(SearchResultActionType::kRemove, u"Remove");
    suggestion_result->SetActions(actions);
  }
  // Give this item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  suggestion_result->SetAccessibleName(base::UTF8ToUTF16(result_id));

  return suggestion_result;
}

// Verifies the current search result page anchored dialog bounds.
// The dialog is expected to be positioned horizontally centered within the
// search box bounds.
void SanityCheckSearchResultsAnchoredDialogBounds(
    const views::Widget* dialog,
    const SearchBoxView* search_box_view) {
  auto horizontal_center_offset = [](const gfx::Rect& inner,
                                     const gfx::Rect& outer) -> int {
    return outer.CenterPoint().x() - inner.CenterPoint().x();
  };

  const gfx::Rect dialog_bounds = dialog->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = search_box_view->GetBoundsInScreen();
  // The dialog should be horizontally centered within the search box.
  EXPECT_EQ(0, horizontal_center_offset(dialog_bounds, search_box_bounds));
  // Verify the confirmation dialog is positioned with the top within search
  // box bounds.
  EXPECT_GT(dialog_bounds.y(), search_box_bounds.y());
  EXPECT_LT(dialog_bounds.y(), search_box_bounds.bottom());
}

// Returns the search box view from either the clamshell bubble or the tablet
// mode fullscreen launcher.
SearchBoxView* GetSearchBoxViewFromHelper(AppListTestHelper* helper) {
  if (!Shell::Get()->IsInTabletMode()) {
    DCHECK(Shell::Get()->app_list_controller()->IsVisible());
    return helper->GetBubbleSearchBoxView();
  }
  return helper->GetSearchBoxView();
}

// Test observer to verify that `AppListView` / its presenter do not call
// `OnVisibilityChanged(false)` during **aborted** hide animation.
class TestAppListControllerObserver : public AppListControllerObserver {
 public:
  TestAppListControllerObserver() = default;
  TestAppListControllerObserver(const TestAppListControllerObserver&) = delete;
  TestAppListControllerObserver& operator=(
      const TestAppListControllerObserver&) = delete;
  ~TestAppListControllerObserver() override {
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  }

  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override {
    if (!shown)
      ++visibility_changed_to_hidden_times_;
  }

  int visibility_changed_to_hidden_times() const {
    return visibility_changed_to_hidden_times_;
  }

 private:
  int visibility_changed_to_hidden_times_ = 0;
};

}  // namespace

// This suite used to be called AppListPresenterDelegateTest. It's not called
// AppListPresenterImplTest because that name was already taken. The two test
// suite were not merged in order to maintain git blame history for this file.
class AppListPresenterTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  AppListPresenterTest() = default;
  AppListPresenterTest(const AppListPresenterTest&) = delete;
  AppListPresenterTest& operator=(const AppListPresenterTest&) = delete;
  ~AppListPresenterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  // Ensures the launcher is visible and showing the apps grid.
  void EnsureLauncherWithVisibleAppsGrid() {
    auto* helper = GetAppListTestHelper();
    helper->ShowAndRunLoop(GetPrimaryDisplayId());
    helper->WaitUntilIdle();
  }

  void SetAppListStateAndWait(AppListViewState new_state) {
    GetAppListView()->SetState(new_state);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(new_state);
  }

  // Whether to run the test with mouse or gesture events.
  bool TestMouseEventParam() const { return GetParam(); }

  // Whether to run the test with fullscreen or not.
  bool TestFullscreenParam() const { return GetParam(); }

  test::AppListTestModel* GetAppListModel() {
    return GetAppListTestHelper()->model();
  }

  SearchBoxView* GetSearchBoxView() {
    return GetSearchBoxViewFromHelper(GetAppListTestHelper());
  }

  gfx::Point GetPointOutsideSearchbox() {
    // Ensures that the point satisfies the following conditions:
    // (1) The point is within AppListView.
    // (2) The point is outside of the search box.
    // (3) The touch event on the point should not be consumed by the handler
    // for back gesture.
    return GetSearchBoxView()->GetBoundsInScreen().bottom_right();
  }

  gfx::Point GetPointInsideSearchbox() {
    return GetSearchBoxView()->GetBoundsInScreen().CenterPoint();
  }

  AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

  SearchResultPageView* search_result_page() {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->search_result_page_view();
  }

  SearchResultContainerView* GetDefaultSearchResultListView() {
    return search_result_page()
        ->search_view()
        ->result_container_views_for_test()[kBestMatchContainerIndex];
  }

  AppsGridView* apps_grid_view() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  void ClickMouseAt(const gfx::Point& point) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(point);
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  }

  void LongPressAt(const gfx::Point& point) {
    ui::GestureEvent long_press(
        point.x(), point.y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureLongPress));
    GetEventGenerator()->Dispatch(&long_press);
  }
};

// Instantiate the values in the parameterized tests. Used to
// toggle mouse and touch events and in some tests to toggle fullscreen mode
// tests.
INSTANTIATE_TEST_SUITE_P(All, AppListPresenterTest, testing::Bool());

// Tests tablet and clamshell mode combinations.
class AppListBubbleAndTabletTestBase : public AshTestBase {
 public:
  explicit AppListBubbleAndTabletTestBase(bool tablet_mode)
      : tablet_mode_(tablet_mode) {}
  AppListBubbleAndTabletTestBase(const AppListBubbleAndTabletTestBase&) =
      delete;
  AppListBubbleAndTabletTestBase& operator=(
      const AppListBubbleAndTabletTestBase&) = delete;
  ~AppListBubbleAndTabletTestBase() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  AppsGridView* GetAppsGridView() {
    if (tablet_mode_param())
      return GetAppListTestHelper()->GetRootPagedAppsGridView();
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  void SetupGridTestApi() {
    grid_test_api_ =
        std::make_unique<test::AppsGridViewTestApi>(GetAppsGridView());
  }

  void OnReorderAnimationDone(base::OnceClosure closure,
                              bool aborted,
                              AppListGridAnimationStatus status) {
    EXPECT_FALSE(aborted);
    EXPECT_EQ(AppListGridAnimationStatus::kReorderFadeIn, status);
    std::move(closure).Run();
  }

  void SortAppList(AppListSortOrder order) {
    tablet_mode_param()
        ? GetAppListTestHelper()
              ->GetAppsContainerView()
              ->UpdateForNewSortingOrder(
                  order,
                  /*animate=*/true,
                  /*update_position_closure=*/base::DoNothing(),
                  /*animation_done_closure=*/base::DoNothing())
        : GetAppListTestHelper()->GetBubbleView()->UpdateForNewSortingOrder(
              order,
              /*animate=*/true, /*update_position_closure=*/base::DoNothing());

    base::RunLoop run_loop;
    GetAppsGridView()->AddReorderCallbackForTest(base::BindRepeating(
        &AppListBubbleAndTabletTestBase::OnReorderAnimationDone,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return tablet_mode_; }

  // Bubble launcher is visible in clamshell mode.
  bool should_show_bubble_launcher() { return !tablet_mode_param(); }

  test::AppListTestModel* GetAppListModel() {
    return GetAppListTestHelper()->model();
  }

  SearchBoxView* GetSearchBoxView() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleSearchBoxView()
               : GetAppListTestHelper()->GetAppListView()->search_box_view();
  }

  SearchResultPageView* GetFullscreenSearchPage() {
    return GetAppListTestHelper()
        ->GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->search_result_page_view();
  }

  bool AppListSearchResultPageVisible() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleSearchPage()->GetVisible()
               : GetFullscreenSearchPage()->GetVisible();
  }

  SearchResultContainerView* GetDefaultSearchResultListView() {
    if (should_show_bubble_launcher()) {
      return GetAppListTestHelper()
          ->GetBubbleAppListSearchView()
          ->result_container_views_for_test()[kBestMatchContainerIndex];
    }
    return GetFullscreenSearchPage()
        ->search_view()
        ->result_container_views_for_test()[kBestMatchContainerIndex];
  }

  ResultSelectionController* GetResultSelectionController() {
    if (should_show_bubble_launcher()) {
      return GetAppListTestHelper()
          ->GetBubbleAppListSearchView()
          ->result_selection_controller_for_test();
    }

    return GetFullscreenSearchPage()
        ->search_view()
        ->result_selection_controller_for_test();
  }

  SearchResultPageAnchoredDialog* GetSearchResultPageDialog() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleSearchPageDialog()
               : GetAppListTestHelper()->GetFullscreenSearchPageDialog();
  }

  void CancelSearchResultPageDialog() {
    views::Widget* widget = GetSearchResultPageDialog()->widget();
    views::WidgetDelegate* widget_delegate = widget->widget_delegate();
    views::test::WidgetDestroyedWaiter widget_waiter(widget);
    GestureTapOn(static_cast<RemoveQueryConfirmationDialog*>(widget_delegate)
                     ->GetCancelButtonForTesting());
    widget_waiter.Wait();
  }

  void AcceptSearchResultPageDialog() {
    views::Widget* widget = GetSearchResultPageDialog()->widget();
    views::WidgetDelegate* widget_delegate = widget->widget_delegate();
    views::test::WidgetDestroyedWaiter widget_waiter(widget);
    GestureTapOn(static_cast<RemoveQueryConfirmationDialog*>(widget_delegate)
                     ->GetAcceptButtonForTesting());
    widget_waiter.Wait();
  }

  ContinueSectionView* GetContinueSectionView() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleContinueSectionView()
               : GetAppListTestHelper()->GetFullscreenContinueSectionView();
  }

  RecentAppsView* GetRecentAppsView() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleRecentAppsView()
               : GetAppListTestHelper()->GetFullscreenRecentAppsView();
  }

  views::View* GetAppsSeparator() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleLauncherAppsSeparatorView()
               : GetAppListTestHelper()
                     ->GetFullscreenLauncherAppsSeparatorView();
  }

  AppListFolderView* GetFolderView() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleFolderView()
               : GetAppListTestHelper()->GetFullscreenFolderView();
  }

  void DeleteFolderItemChildren(AppListFolderItem* item) {
    std::vector<std::string> items_to_delete;
    for (size_t i = 0; i < item->ChildItemCount(); ++i) {
      items_to_delete.push_back(item->GetChildItemAt(i)->id());
    }
    for (auto& item_to_delete : items_to_delete)
      GetAppListModel()->DeleteItem(item_to_delete);
  }

  void LongPressAt(const gfx::Point& point) {
    ui::GestureEvent long_press(
        point.x(), point.y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureLongPress));
    GetEventGenerator()->Dispatch(&long_press);
  }

  void EnsureBubbleLauncherShown() {
    Shell::Get()->app_list_controller()->bubble_presenter_for_test()->Show(
        GetPrimaryDisplay().id());
  }

  void EnsureLauncherShown() {
    const bool in_tablet_mode = Shell::Get()->IsInTabletMode();

    // App list always visible in tablet mode, so launcher needs to explicitly
    // be shown only when in clamshell mode.
    if (!in_tablet_mode)
      EnsureBubbleLauncherShown();

    auto* helper = GetAppListTestHelper();
    if (!in_tablet_mode) {
      apps_grid_view_ = helper->GetScrollableAppsGridView();
    } else {
      apps_grid_view_ = helper->GetRootPagedAppsGridView();
    }
    DCHECK(apps_grid_view_);
  }

  gfx::Point SearchBoxCenterPoint() {
    SearchBoxView* search_box_view =
        should_show_bubble_launcher()
            ? GetAppListTestHelper()->GetBubbleSearchBoxView()
            : GetAppListTestHelper()->GetAppListView()->search_box_view();
    return search_box_view->GetBoundsInScreen().CenterPoint();
  }

  AppListFolderView* folder_view() {
    auto* helper = GetAppListTestHelper();
    return should_show_bubble_launcher() ? helper->GetBubbleFolderView()
                                         : helper->GetFullscreenFolderView();
  }

  bool AppListIsInFolderView() {
    return GetAppListTestHelper()->IsInFolderView();
  }

 protected:
  const bool tablet_mode_;

  std::unique_ptr<test::AppsGridViewTestApi> grid_test_api_;
  raw_ptr<AppsGridView, DanglingUntriaged> apps_grid_view_ = nullptr;
};

// Parameterized by tablet/clamshell mode.
class AppListBubbleAndTabletTest : public AppListBubbleAndTabletTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  AppListBubbleAndTabletTest()
      : AppListBubbleAndTabletTestBase(/*tablet_mode=*/GetParam()) {}
  AppListBubbleAndTabletTest(const AppListBubbleAndTabletTest&) = delete;
  AppListBubbleAndTabletTest& operator=(const AppListBubbleAndTabletTest&) =
      delete;
  ~AppListBubbleAndTabletTest() override = default;
};

// Instantiate the values in the parameterized tests. The boolean
// determines whether to run the test in tablet mode.
INSTANTIATE_TEST_SUITE_P(TabletMode,
                         AppListBubbleAndTabletTest,
                         testing::Bool());

// Subclass suite to test drag specific behavior, parameterized by
// tablet/clamshell mode and drag and whether drag and drop refactor is enabled.
class AppListBubbleAndTabletDragTest
    : public AppListBubbleAndTabletTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AppListBubbleAndTabletDragTest()
      : AppListBubbleAndTabletTestBase(
            /*tablet_mode=*/GetParam()) {}
  AppListBubbleAndTabletDragTest(const AppListBubbleAndTabletDragTest&) =
      delete;
  AppListBubbleAndTabletDragTest& operator=(
      const AppListBubbleAndTabletDragTest&) = delete;
  ~AppListBubbleAndTabletDragTest() override = default;
};

// Instantiate the values in the parameterized tests. The boolean
// determines whether to run the test in tablet mode.
INSTANTIATE_TEST_SUITE_P(TabletMode,
                         AppListBubbleAndTabletDragTest,
                         testing::Bool());

// Tests only tablet mode.
class AppListTabletTest : public AppListBubbleAndTabletTestBase {
 public:
  AppListTabletTest() : AppListBubbleAndTabletTestBase(/*tablet_mode=*/true) {}
  AppListTabletTest(const AppListTabletTest&) = delete;
  AppListTabletTest& operator=(const AppListTabletTest&) = delete;
  ~AppListTabletTest() override = default;
};

// Used to test app_list behavior with a populated apps_grid.
class PopulatedAppListTest : public AshTestBase {
 public:
  PopulatedAppListTest() {}
  ~PopulatedAppListTest() override = default;

  void SetUp() override {
    AppListConfigProvider::Get().ResetForTesting();
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    // Fullscreen launcher is used only in tablet mode, so enable tablet mode.
    EnableTabletMode(true);
  }

 protected:
  void OpenAppListInFullscreen() {
    AppListPresenterImpl* presenter =
        Shell::Get()->app_list_controller()->fullscreen_presenter();
    presenter->Show(AppListViewState::kFullscreenAllApps,
                    GetPrimaryDisplay().id(), base::TimeTicks::Now(),
                    /*show_source=*/std::nullopt);
    app_list_view_ = presenter->GetView();
  }

  void InitializeAppsGrid() {
    if (!app_list_view_)
      OpenAppListInFullscreen();
    apps_grid_view_ = app_list_view_->app_list_main_view()
                          ->contents_view()
                          ->apps_container_view()
                          ->apps_grid_view();
    apps_grid_test_api_ =
        std::make_unique<test::AppsGridViewTestApi>(apps_grid_view_);
  }

  void PopulateApps(int n) {
    GetAppListModel()->PopulateApps(n);
    app_list_view_->GetWidget()->LayoutRootViewIfNecessary();
  }

  AppListFolderItem* CreateAndPopulateFolderWithApps(int n) {
    auto* folder = GetAppListModel()->CreateAndPopulateFolderWithApps(n);
    app_list_view_->GetWidget()->LayoutRootViewIfNecessary();
    return folder;
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) {
    DCHECK_GT(GetAppListModel()->top_level_item_list()->item_count(), 0u);
    return apps_grid_test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  bool AppListIsInFolderView() const {
    return app_list_view_->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->IsInFolderView();
  }

  AppListFolderView* folder_view() {
    return app_list_view_->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->app_list_folder_view();
  }

  void UpdateFolderName(const std::string& name) {
    std::u16string folder_name = base::UTF8ToUTF16(name);
    folder_view()->folder_header_view()->SetFolderNameForTest(folder_name);
    folder_view()->folder_header_view()->ContentsChanged(
        folder_view()->folder_header_view()->GetFolderNameViewForTest(),
        folder_name);
  }

  test::AppListTestModel* GetAppListModel() {
    return GetAppListTestHelper()->model();
  }

  const std::string GetFolderName() {
    return base::UTF16ToUTF8(
        folder_view()->folder_header_view()->GetFolderNameForTest());
  }

  void RefreshFolderName() {
    folder_view()->folder_header_view()->ItemNameChanged();
  }

  void RotateScreen() {
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::RotationSource::ACTIVE);
    // AppListView is usually notified of display bounds changes by
    // AppListPresenter, though the test delegate implementation does not
    // track display metrics changes, so OnParentWindowBoundsChanged() has to be
    // explicitly called here.
    app_list_view_->OnParentWindowBoundsChanged();
  }

  std::unique_ptr<test::AppsGridViewTestApi> apps_grid_test_api_;
  raw_ptr<AppListView, DanglingUntriaged> app_list_view_ =
      nullptr;  // Owned by native widget.
  raw_ptr<PagedAppsGridView, DanglingUntriaged> apps_grid_view_ =
      nullptr;  // Owned by |app_list_view_|.
};

// Subclass of PopulatedAppListTest which enables the virtual keyboard.
class PopulatedAppListWithVKEnabledTest : public PopulatedAppListTest {
 public:
  PopulatedAppListWithVKEnabledTest() = default;
  ~PopulatedAppListWithVKEnabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    PopulatedAppListTest::SetUp();
  }
};

// Subclass of PopulatedAppListTest which tests the apps grid drag behavior
// interrumpted during a screen rotation. Enables drag and drop refactor by
// default.
class PopulatedAppListScreenRotationTest : public PopulatedAppListTest {
 public:
  PopulatedAppListScreenRotationTest() = default;
  ~PopulatedAppListScreenRotationTest() override = default;
};

// Verify that open folders are closed after sorting apps grid.
TEST_P(AppListBubbleAndTabletTest, SortingClosesOpenFolderView) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetAppListModel()->CreateAndPopulateFolderWithApps(4);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  SetupGridTestApi();

  grid_test_api_->PressItemAt(0);
  EXPECT_TRUE(AppListIsInFolderView());

  SortAppList(AppListSortOrder::kNameAlphabetical);
  EXPECT_FALSE(AppListIsInFolderView());
}

// Tests that folder item view does not animate out and in after folder is
// closed (and the folder item location in apps grid did not change while the
// folder was shown).
TEST_P(AppListBubbleAndTabletTest,
       FolderItemViewNotAnimatingAfterClosingFolder) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Cache the initial folder item bounds.
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* const folder_view = GetFolderView();

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(2);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      GetFolderView()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  base::RunLoop folder_animation_waiter;
  // Once folder completes hiding, the folder item view should be moved to
  // target location.
  folder_view->SetAnimationDoneTestCallback(base::BindLambdaForTesting([&]() {
    folder_animation_waiter.Quit();

    EXPECT_EQ(original_folder_item_bounds,
              folder_item_view->GetBoundsInScreen());

    // The folder item position did not change, so the item view should not
    // start fading out when the folder view hides.
    EXPECT_FALSE(folder_item_view->layer());
  }));
  folder_animation_waiter.Run();

  EXPECT_FALSE(AppListIsInFolderView());

  // Verify that the folder is visible, and positioned in its final bounds.
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // No item layers are expected to be created.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder view bounds do not change if an item gets added to app list
// model while the folder view is visible (even if it changes the folder item
// view position in the root apps grid).
TEST_P(AppListBubbleAndTabletTest,
       FolderViewRemainsInPlaceWhenAddingItemToModel) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* const folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect final_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Add a new item.
  test::AppListTestModel::AppListTestItem* new_item =
      model->CreateItem("new_test_item");
  new_item->SetPosition(GetAppListTestHelper()
                            ->model()
                            ->top_level_item_list()
                            ->item_at(0)
                            ->position()
                            .CreateBefore());
  model->AddItem(new_item);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  grid_test_api_->WaitForItemMoveAnimationDone();

  // Verify that the folder view location did not change.
  EXPECT_EQ(folder_bounds, GetFolderView()->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(3);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 2 should be laid out right of the folder while the folder
  // is shown.
  EXPECT_LT(original_folder_item_bounds.right(),
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().x());

  // The item at slot 1 should be laid out left of the folder.
  EXPECT_GT(original_folder_item_bounds.x(),
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().right());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      GetFolderView()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  base::RunLoop folder_animation_waiter;
  // Once folder completes hiding, the folder item view should be moved to
  // target location.
  folder_view->SetAnimationDoneTestCallback(base::BindLambdaForTesting([&]() {
    folder_animation_waiter.Quit();

    EXPECT_EQ(original_folder_item_bounds,
              folder_item_view->GetBoundsInScreen());

    // The folder item should start fading out in it's current position.
    ASSERT_TRUE(folder_item_view->layer());
    EXPECT_EQ(0.0f, folder_item_view->layer()->GetTargetOpacity());
  }));

  folder_animation_waiter.Run();

  EXPECT_FALSE(AppListIsInFolderView());

  // Wait for the folder item to fade out.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  grid_test_api_->WaitForItemMoveAnimationDone();

  // Make sure the folder item view fade in animation is done.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  // Verify that the folder is visible, and positioned in its final bounds.
  EXPECT_EQ(final_folder_item_bounds, folder_item_view->GetBoundsInScreen());
  EXPECT_FALSE(folder_item_view->layer());

  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder view bounds do not change if position of the original
// folder item view changes in the model (as long as the folder is open).
TEST_P(AppListBubbleAndTabletTest,
       FolderViewRemainsInPlaceWhenItemMovedToEndInModel) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* const folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();
  const gfx::Rect final_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(5)->GetBoundsInScreen();

  // Move the folder item to the last position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(5)
                                   ->position()
                                   .CreateAfter(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(5);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 2 in the model should remain at slot 3 (where it was
  // before folder item moved in the model).
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // The item at slot 1 should be remain in place.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(folder_view->GetBoundsInScreen().right_center() +
                               gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  base::RunLoop folder_animation_waiter;
  // Once folder completes hiding, the folder item view should be moved to
  // target location.
  folder_view->SetAnimationDoneTestCallback(base::BindLambdaForTesting([&]() {
    folder_animation_waiter.Quit();

    EXPECT_EQ(original_folder_item_bounds,
              folder_item_view->GetBoundsInScreen());

    // The folder item should start fading out in it's current position.
    ASSERT_TRUE(folder_item_view->layer());
    EXPECT_EQ(0.0f, folder_item_view->layer()->GetTargetOpacity());
  }));

  folder_animation_waiter.Run();

  EXPECT_FALSE(AppListIsInFolderView());

  // Wait for the folder item to fade out.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  grid_test_api_->WaitForItemMoveAnimationDone();

  // Make sure the folder item view fade in animation is done.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  // Verify that the folder is visible, and positioned in its final bounds.
  EXPECT_EQ(final_folder_item_bounds, folder_item_view->GetBoundsInScreen());
  EXPECT_FALSE(folder_item_view->layer());

  // The item at slot 1 should be remain in place.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  // The item at slot 2 in the model should move into original folder item slot.
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder view bounds do not change if position of the original
// folder item view changes in the model (as long as the folder is open).
TEST_P(AppListBubbleAndTabletTest,
       FolderViewRemainsInPlaceWhenItemMovedToStartInModel) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* const folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();
  const gfx::Rect final_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen();

  // Move the folder item to the last position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(0)
                                   ->position()
                                   .CreateBefore(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 3 in the model did not change, so it should remain in
  // place.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());

  // The item at slot 2 in the model should remain in the old position (slot 1).
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(folder_view->GetBoundsInScreen().right_center() +
                               gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  base::RunLoop folder_animation_waiter;
  // Once folder completes hiding, the folder item view should be moved to
  // target location.
  folder_view->SetAnimationDoneTestCallback(base::BindLambdaForTesting([&]() {
    folder_animation_waiter.Quit();

    EXPECT_EQ(original_folder_item_bounds,
              folder_item_view->GetBoundsInScreen());

    // The folder item should start fading out in it's current position.
    ASSERT_TRUE(folder_item_view->layer());
    EXPECT_EQ(0.0f, folder_item_view->layer()->GetTargetOpacity());
  }));

  folder_animation_waiter.Run();

  EXPECT_FALSE(AppListIsInFolderView());

  // Wait for the folder item to fade out.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  grid_test_api_->WaitForItemMoveAnimationDone();

  // Make sure the folder item view fade in animation is done.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  // Verify that the folder is visible, and positioned in its final bounds.
  EXPECT_EQ(final_folder_item_bounds, folder_item_view->GetBoundsInScreen());
  EXPECT_FALSE(folder_item_view->layer());

  // The item at slot 2 in the model should move into original folder item slot.
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  // The item at slot 3 in the model should move into new position.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder item deletion during folder view hide animation is handled
// well.
TEST_P(AppListBubbleAndTabletTest,
       ReorderedFolderItemDeletionDuringFolderClose) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* const folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Move the folder item to the first position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(0)
                                   ->position()
                                   .CreateBefore(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 3 in the model did not change, so it should remain in
  // place.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
  // The item at slot 2 in the model should remain in the old position (slot 1).
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(folder_view->GetBoundsInScreen().right_center() +
                               gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  // Delete the folder item while the folder is animating out.
  DeleteFolderItemChildren(folder_item);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_FALSE(AppListIsInFolderView());

  // Verify remaining items are moved into correct slots.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder item deletion just after folder gets hidden (while item
// bounds are still animating to final positions) gets handled well.
TEST_P(AppListBubbleAndTabletTest,
       ReorderedFolderItemDeletionDuringFolderItemFadeOut) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Move the folder item to the last position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(0)
                                   ->position()
                                   .CreateBefore(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 3 in the model did not change, so it should remain in
  // place.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
  // The item at slot 2 in the model should remain in the old position (slot 1).
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(folder_view->GetBoundsInScreen().right_center() +
                               gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  base::RunLoop folder_animation_waiter;
  // Once folder completes hiding, the folder item view should be moved to
  // target location.
  folder_view->SetAnimationDoneTestCallback(base::BindLambdaForTesting([&]() {
    folder_animation_waiter.Quit();

    // Delete the folder item while items are animating into their final
    // positions.
    DeleteFolderItemChildren(folder_item);
  }));

  folder_animation_waiter.Run();

  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_FALSE(AppListIsInFolderView());

  // Verify remaining items are moved into correct slots.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder item deletion just after folder gets hidden (while item
// bounds are still animating to final positions) gets handled well.
TEST_P(AppListBubbleAndTabletTest,
       ReorderedFolderItemDeletionAfterFolderItemFadeOut) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Move the folder item to the last position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(0)
                                   ->position()
                                   .CreateBefore(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 3 in the model did not change, so it should remain in
  // place.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
  // The item at slot 2 in the model should remain in the old position (slot 1).
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(folder_view->GetBoundsInScreen().right_center() +
                               gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();

  EXPECT_TRUE(folder_view->IsAnimationRunning());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  GetAppListTestHelper()->WaitForFolderAnimation();

  // Wait for the folder item to fade out.
  if (folder_item_view->layer()) {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(folder_item_view->layer());
  }

  // Delete the folder item while items are animating into their final
  // positions.
  DeleteFolderItemChildren(folder_item);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_FALSE(AppListIsInFolderView());

  // Verify remaining items are moved into correct slots.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());

  // Verify that item view layers have been deleted.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that folder item deletion while the folder is shown gets handled well.
TEST_P(AppListBubbleAndTabletTest,
       ReorderedFolderItemDeletionWhileFolderShown) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());
  GetAppListTestHelper()->WaitForFolderAnimation();
  AppListFolderView* folder_view = GetFolderView();

  // Cache the initial folder bounds.
  const gfx::Rect folder_bounds = folder_view->GetBoundsInScreen();
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Move the folder item to the last position in the model.
  model->RequestPositionUpdate(folder_id,
                               GetAppListTestHelper()
                                   ->model()
                                   ->top_level_item_list()
                                   ->item_at(0)
                                   ->position()
                                   .CreateBefore(),
                               RequestPositionUpdateReason::kMoveItem);

  // Verify that the folder view location did not actually change.
  EXPECT_EQ(folder_bounds, folder_view->GetBoundsInScreen());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_TRUE(folder_item_view);
  ASSERT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(original_folder_item_bounds, folder_item_view->GetBoundsInScreen());

  // The item at slot 3 in the model did not change, so it should remain in
  // place.
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
  // The item at slot 2 in the model should remain in the old position (slot 1).
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());

  // Delete the folder item while it's still shown.
  DeleteFolderItemChildren(folder_item);

  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_FALSE(AppListIsInFolderView());

  // Verify remaining items are moved into correct slots.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
}

// Tests that folder item deletion while the folder view is still animating into
// shown state gets handled well.
TEST_P(AppListBubbleAndTabletTest, ReorderedFolderItemDeletionDuringShow) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* const folder_item =
      model->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  model->PopulateApps(3);

  // Setup tablet/clamshell mode and show launcher.
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  SetupGridTestApi();

  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  grid_test_api_->PressItemAt(2);
  EXPECT_TRUE(AppListIsInFolderView());

  // Cache the initial folder bounds.
  const gfx::Rect original_folder_item_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();
  const gfx::Rect original_item_1_bounds =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen();
  const gfx::Rect original_item_3_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Delete the folder item while the folder is still showing.
  DeleteFolderItemChildren(folder_item);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_FALSE(AppListIsInFolderView());

  // Verify remaining items are moved into correct slots.
  EXPECT_EQ(original_item_1_bounds,
            apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen());
  EXPECT_EQ(original_folder_item_bounds,
            apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen());
  EXPECT_EQ(original_item_3_bounds,
            apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen());
}

// Tests that Zero State Search is only shown when needed.
TEST_P(AppListBubbleAndTabletTest, LauncherSearchZeroState) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tap Search Box to activate it and check search result view visibility.
  generator->GestureTapAt(SearchBoxCenterPoint());
  EXPECT_FALSE(AppListSearchResultPageVisible());

  // Type a character into the textfield and check visibility.
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Delete the character in the textfield and check visibility.
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_FALSE(AppListSearchResultPageVisible());
}

// Verifies that changes in launcher search box do not cause duplicate search
// requests if both clamshell and tablet app list views exist (and one of them
// is hidden).
TEST_P(AppListBubbleAndTabletTest, NoDuplicateSearchRequests) {
  // Toggle tablet mode to ensure the app list view for tablet mode state
  // opposite to the one used in test is created.
  EnableTabletMode(!tablet_mode_param());
  EnsureLauncherShown();
  EnableTabletMode(tablet_mode_param());

  EnsureLauncherShown();

  // Type a character into the textfield and verify this issues a single search
  // request.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  TestAppListClient* const client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());
  generator->PressKey(ui::VKEY_B, 0);
  EXPECT_EQ(std::vector<std::u16string>({u"ab"}),
            client->GetAndResetPastSearchQueries());
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_EQ(std::vector<std::u16string>({u""}),
            client->GetAndResetPastSearchQueries());
}

TEST_P(AppListBubbleAndTabletTest, ClearSearchButtonClearsSearch) {
  // Toggle tablet mode to ensure the app list view for tablet mode state
  // opposite to the one used in test is created.
  EnableTabletMode(!tablet_mode_param());
  EnsureLauncherShown();
  EnableTabletMode(tablet_mode_param());

  EnsureLauncherShown();

  // Type a character into the textfield and verify this issues a single search
  // request.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  TestAppListClient* const client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());
  generator->PressKey(ui::VKEY_B, 0);
  EXPECT_EQ(std::vector<std::u16string>({u"ab"}),
            client->GetAndResetPastSearchQueries());

  SearchBoxView* search_box_view = GetSearchBoxView();
  search_box_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(
      search_box_view->filter_and_close_button_container()->GetVisible());
  LeftClickOn(search_box_view->close_button());

  EXPECT_EQ(std::vector<std::u16string>({u""}),
            client->GetAndResetPastSearchQueries());
}

// Regression test for b/204482740.
TEST_P(AppListBubbleAndTabletTest, AppListEventTargeterForAssistantScrolling) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // A custom event targeter is installed.
  aura::Window* window = Shell::Get()->app_list_controller()->GetWindow();
  ASSERT_TRUE(window);
  aura::WindowTargeter* targeter = window->targeter();
  ASSERT_TRUE(targeter);

  // Simulate an assistant card with a webview being shown, which sets a window
  // property on its window. See AssistantCardElementView::AddedToWidget().
  aura::Window* child =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(100, 100), window);
  child->SetProperty(assistant::ui::kOnlyAllowMouseClickEvents, true);

  // Scroll events are blocked for that window.
  constexpr int offset = 10;
  ui::ScrollEvent scroll_down(ui::EventType::kScroll, gfx::Point(),
                              base::TimeTicks::Now(), ui::EF_NONE, 0, offset, 0,
                              offset, /*finger_count=*/2);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(child, scroll_down));

  // Click events are not blocked.
  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks::Now(), ui::EF_NONE,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                         gfx::Point(), base::TimeTicks::Now(), ui::EF_NONE,
                         ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(child, press));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(child, press));
}

// Tests that apps container/page does not have a separator between apps grid
// and recent apps/continue section if neither continue section nor recent apps
// are shown.
TEST_P(AppListBubbleAndTabletTest,
       NoSeparatorWithoutRecentAppsOrContinueSection) {
  GetAppListTestHelper()->AddAppItems(5);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  views::View* separator = GetAppsSeparator();
  EXPECT_TRUE(separator);

  RecentAppsView* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps);

  ContinueSectionView* continue_section = GetContinueSectionView();
  EXPECT_TRUE(continue_section);

  EXPECT_FALSE(separator->GetVisible());
  EXPECT_FALSE(recent_apps->GetVisible());
  EXPECT_FALSE(continue_section->GetVisible());

  // If some content gets added to continue section, separator is expected to
  // show.
  GetAppListTestHelper()->AddContinueSuggestionResults(3);
  // Run loop, as continue section content is updated asynchronously.
  base::RunLoop().RunUntilIdle();

  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(continue_section->GetVisible());
  EXPECT_FALSE(recent_apps->GetVisible());
}

// Tests that apps container/page has a separator between apps grid
// and recent apps/continue section if recent apps are shown.
TEST_P(AppListBubbleAndTabletTest, SeparatorShownWithRecentApps) {
  GetAppListTestHelper()->AddAppItems(5);
  GetAppListTestHelper()->AddRecentApps(4);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  views::View* separator = GetAppsSeparator();
  EXPECT_TRUE(separator);

  RecentAppsView* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps);

  ContinueSectionView* continue_section = GetContinueSectionView();
  EXPECT_TRUE(continue_section);

  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(recent_apps->GetVisible());
  EXPECT_FALSE(continue_section->GetVisible());
}

// Tests that apps container/page has a separator between apps grid
// and recent apps/continue section if continue section is shown.
TEST_P(AppListBubbleAndTabletTest, SeparatorShownWithContinueSection) {
  GetAppListTestHelper()->AddAppItems(5);
  GetAppListTestHelper()->AddContinueSuggestionResults(4);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  views::View* separator = GetAppsSeparator();
  EXPECT_TRUE(separator);

  RecentAppsView* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps);

  ContinueSectionView* continue_section = GetContinueSectionView();
  EXPECT_TRUE(continue_section);

  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(continue_section->GetVisible());
  EXPECT_FALSE(recent_apps->GetVisible());
}

// Tests that apps container/page has a separator between apps grid
// and recent apps/continue section if recent apps and continue section are
// shown.
TEST_P(AppListBubbleAndTabletTest,
       SeparatorShownWithContinueSectionAndRecentApps) {
  GetAppListTestHelper()->AddAppItems(5);
  GetAppListTestHelper()->AddContinueSuggestionResults(4);
  GetAppListTestHelper()->AddRecentApps(4);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  views::View* separator = GetAppsSeparator();
  EXPECT_TRUE(separator);

  RecentAppsView* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps);

  ContinueSectionView* continue_section = GetContinueSectionView();
  EXPECT_TRUE(continue_section);

  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(recent_apps->GetVisible());
  EXPECT_TRUE(continue_section->GetVisible());
}

// Test that the separator is centered between recent apps and the first row
// of the apps grid, when recent apps are shown.
TEST_F(AppListTabletTest, SeparatorCenteredBetweenRecentAppsAndAppsGrid) {
  GetAppListTestHelper()->AddAppItems(5);
  GetAppListTestHelper()->AddContinueSuggestionResults(3);
  EnableTabletMode(true);

  views::View* separator = GetAppsSeparator();
  AppsContainerView* apps_container =
      GetAppListTestHelper()->GetAppsContainerView();

  // Separator is not centered and should be positioned below the continue
  // section, because recent apps are not shown.
  EXPECT_EQ(separator->GetBoundsInScreen().y(),
            GetContinueSectionView()->GetBoundsInScreen().bottom() +
                separator->GetProperty(views::kMarginsKey)->height() / 2);

  // Add recent apps and layout to update the separator's bounds.
  GetAppListTestHelper()->AddRecentApps(4);
  apps_container->ResetForShowApps();
  GetAppsGridView()->GetWidget()->LayoutRootViewIfNecessary();
  views::View* recent_apps = GetRecentAppsView();

  SetupGridTestApi();
  const AppListItemView* first_row_item =
      grid_test_api_->GetViewAtIndex(GridIndex(0, 0));

  // Separator should be centered between the bottom of recent apps and the
  // top of the first row of the apps grid.
  const int centered_y = recent_apps->GetBoundsInScreen().bottom() +
                         (first_row_item->GetBoundsInScreen().y() -
                          recent_apps->GetBoundsInScreen().bottom()) /
                             2;
  EXPECT_EQ(centered_y, separator->GetBoundsInScreen().y());
}

// Verifies that tapping on the search box in tablet mode with animation and
// zero state enabled should not bring Chrome crash (https://crbug.com/958267).
TEST_P(AppListPresenterTest, ClickSearchBoxInTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Necessary for AppListView::StateAnimationMetricsReporter::Report being
  // called when animation ends.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Gesture tap on the search box.
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Gesture tap on the area out of search box.
  generator->GestureTapAt(GetPointOutsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(AppListBubbleAndTabletTest, RemoveSuggestionShowsConfirmDialog) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(CreateOmniboxSuggestionResult(
      "Another suggestion", /*support_removal=*/true));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  // The result list is updated asynchronously.
  base::RunLoop().RunUntilIdle();

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Make sure the search results page is laid out after adding result action
  // buttons.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_TRUE(result_view->actions_view());
  EXPECT_EQ(1u, result_view->actions_view()->children().size());
  views::View* const action_view = result_view->actions_view()->children()[0];

  // The remove action button is visible on hover only.
  EXPECT_FALSE(action_view->GetVisible());
  generator->MoveMouseTo(result_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(action_view->GetVisible());

  // Record the current result selection before clicking the remove action
  // button.
  ResultSelectionController* result_selection_controller =
      GetResultSelectionController();
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  ResultLocationDetails* result_location =
      result_selection_controller->selected_location_details();

  // Ensure layout after the action view visibility has been updated.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  // Click the remove action button, this should surface a confirmation dialog.
  LeftClickOn(action_view);

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());
  ASSERT_TRUE(GetSearchResultPageDialog());

  // Cancel the dialog - the app list should remain in the search result page,
  // the suggestion removal dialog should be hidden, and no result action should
  // be invoked.
  CancelSearchResultPageDialog();

  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_FALSE(GetSearchResultPageDialog());
  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());

  // The result selection should be at the same position.
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_EQ(*result_location,
            *result_selection_controller->selected_location_details());

  // Make sure that the action view is shown.
  generator->MoveMouseTo(action_view->GetBoundsInScreen().left_center());
  EXPECT_TRUE(action_view->GetVisible());

  // Ensure layout after the action view visibility has been updated.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  // Click remove suggestion action button again.
  generator->MoveMouseTo(action_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Expect the removal confirmation dialog - this time, accept it.
  ASSERT_TRUE(GetSearchResultPageDialog());
  AcceptSearchResultPageDialog();

  // The app list should remain showing search results, the dialog should be
  // closed, and result removal action should be invoked.
  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_FALSE(GetSearchResultPageDialog());

  // A result should still be selected.
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());

  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {kTestResultId, SearchResultActionType::kRemove}};
  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      GetAppListTestHelper()
          ->app_list_client()
          ->GetAndResetInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(AppListBubbleAndTabletTest, RemoveSuggestionUsingLongTap) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(CreateOmniboxSuggestionResult(
      "Another suggestion", /*support_removal=*/true));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Make sure the search results page is laid out after adding result action
  // buttons.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_TRUE(result_view->actions_view());
  EXPECT_EQ(1u, result_view->actions_view()->children().size());
  views::View* const action_view = result_view->actions_view()->children()[0];

  EXPECT_FALSE(action_view->GetVisible());
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(action_view->GetVisible());

  // Ensure layout after the action view visibility has been updated.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  // Click the remove action button, this should surface a confirmation dialog.
  LeftClickOn(action_view);

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());
  ASSERT_TRUE(GetSearchResultPageDialog());

  // Cancel the dialog - the app list should remain in the search result page,
  // the suggestion removal dialog should be hidden, and no result action should
  // be invoked.
  CancelSearchResultPageDialog();

  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_FALSE(GetSearchResultPageDialog());

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());
  EXPECT_FALSE(result_view->selected());

  // Long tap on the result again.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());

  // Ensure layout after the action view visibility has been updated.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  // Click the remove action button, this should surface a confirmation dialog.
  LeftClickOn(action_view);

  // Expect the removal confirmation dialog - this time, accept it.
  ASSERT_TRUE(GetSearchResultPageDialog());
  AcceptSearchResultPageDialog();

  // The app list should remain showing search results, the dialog should be
  // closed, and result removal action should be invoked.
  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_FALSE(GetSearchResultPageDialog());
  EXPECT_FALSE(result_view->selected());

  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {kTestResultId, SearchResultActionType::kRemove}};

  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      GetAppListTestHelper()
          ->app_list_client()
          ->GetAndResetInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(AppListBubbleAndTabletTest, RemoveSuggestionUsingKeyboard) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(CreateOmniboxSuggestionResult(
      "Another suggestion", /*support_removal=*/true));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  GetAppListTestHelper()->WaitUntilIdle();

  // Select a removable suggestion.
  generator->PressKey(ui::VKEY_DOWN, 0);

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());
  ASSERT_TRUE(result_view->selected());
  ASSERT_TRUE(result_view->actions_view());
  EXPECT_EQ(1u, result_view->actions_view()->children().size());

  // Press shortcut to delete the result.
  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN);

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());
  ASSERT_TRUE(GetSearchResultPageDialog());

  // Expect the removal confirmation dialog - accept it.
  ASSERT_TRUE(GetSearchResultPageDialog());
  AcceptSearchResultPageDialog();

  // The app list should remain showing search results, the dialog should be
  // closed, and result removal action should be invoked.
  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_FALSE(GetSearchResultPageDialog());
  EXPECT_FALSE(result_view->selected());

  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {kTestResultId, SearchResultActionType::kRemove}};

  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      GetAppListTestHelper()
          ->app_list_client()
          ->GetAndResetInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(AppListBubbleAndTabletTest,
       SuggestionRemoveShortcutOnViewWithNoRemovalAction) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add suggestion results.
  GetSearchModel()->results()->Add(CreateOmniboxSuggestionResult(
      "Another suggestion", /*support_removal=*/false));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/false));
  GetAppListTestHelper()->WaitUntilIdle();

  generator->PressKey(ui::VKEY_DOWN, 0);

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());
  ASSERT_TRUE(result_view->selected());

  // Press shortcut to delete the result.
  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN);

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndResetInvokedResultActions()
                  .empty());
  EXPECT_FALSE(GetSearchResultPageDialog());

  EXPECT_TRUE(AppListSearchResultPageVisible());
  EXPECT_EQ(u"a", GetSearchBoxView()->search_box()->GetText());

  result_view = GetDefaultSearchResultListView()->GetResultViewAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  EXPECT_EQ(kTestResultId, result_view->result()->id());
  EXPECT_TRUE(result_view->selected());
}

TEST_P(AppListBubbleAndTabletTest,
       TransitionToAppsContainerClosesRemoveSuggestionDialog) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add a zero state suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Show remove suggestion dialog.
  result_view->GetWidget()->LayoutRootViewIfNecessary();
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());
  result_view->GetWidget()->LayoutRootViewIfNecessary();
  LeftClickOn(result_view->actions_view()->children()[0]);

  ASSERT_TRUE(GetSearchResultPageDialog());

  views::Widget* const confirmation_dialog =
      GetSearchResultPageDialog()->widget();
  ASSERT_TRUE(confirmation_dialog);

  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog,
                                               GetSearchBoxView());

  // Verify that transition to apps page hides the removal confirmation dialog.
  views::test::WidgetDestroyedWaiter widget_close_waiter(confirmation_dialog);
  GetSearchBoxView()->ClearSearchAndDeactivateSearchBox();
  EXPECT_FALSE(AppListSearchResultPageVisible());

  widget_close_waiter.Wait();
}

TEST_P(AppListBubbleAndTabletTest,
       RemoveSuggestionDialogBoundsUpdateWhenVKHidden) {
  // Enable virtual keyboard for this test.
  KeyboardController* const keyboard_controller =
      Shell::Get()->keyboard_controller();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add a suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view =
      GetDefaultSearchResultListView()->GetResultViewAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  auto* const keyboard_ui_controller = keyboard::KeyboardUIController::Get();
  keyboard_ui_controller->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Show remove suggestion dialog.
  result_view->GetWidget()->LayoutRootViewIfNecessary();
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());

  // Ensure layout after the action view visibility has been updated.
  result_view->GetWidget()->LayoutRootViewIfNecessary();

  // Click the remove action button, this should surface a confirmation dialog.
  LeftClickOn(result_view->actions_view()->children()[0]);

  ASSERT_TRUE(GetSearchResultPageDialog());

  // The search box should have lost the focus, which should have hidden the
  // keyboard.
  EXPECT_FALSE(keyboard_ui_controller->IsKeyboardVisible());

  // Sanity check the confirmation dialog bounds (hiding the keyboard might have
  // changed the position of the search box - the confirmation dialog should
  // have followed it).
  views::Widget* const confirmation_dialog =
      GetSearchResultPageDialog()->widget();
  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog,
                                               GetSearchBoxView());

  views::test::WidgetDestroyedWaiter widget_close_waiter(confirmation_dialog);
  GetSearchBoxView()->ClearSearchAndDeactivateSearchBox();
  EXPECT_FALSE(AppListSearchResultPageVisible());
  if (tablet_mode_param())
    EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // Exiting the search results page should close the dialog.
  widget_close_waiter.Wait();
}

// Verifies that rotating the screen when launcher is shown does not crash.
TEST_P(AppListBubbleAndTabletTest, RotationAnimationSmoke) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(15);
  model->CreateAndPopulateFolderWithApps(3);
  model->PopulateApps(15);

  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ScreenRotationAnimator* animator =
      DisplayConfigurationControllerTestApi(
          Shell::Get()->display_configuration_controller())
          .GetScreenRotationAnimatorForDisplay(display.id());
  animator->Rotate(display::Display::ROTATE_90,
                   display::Display::RotationSource::USER,
                   DisplayConfigurationController::ANIMATION_SYNC);
}

// Verifies that rotating the screen and shutting down when the launcher is
// shown does not crash.
TEST_P(AppListBubbleAndTabletTest, ShutdownDuringRotationAnimationSmoke) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(1);
  model->CreateAndPopulateFolderWithApps(3);
  model->PopulateApps(1);

  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ScreenRotationAnimator* animator =
      DisplayConfigurationControllerTestApi(
          Shell::Get()->display_configuration_controller())
          .GetScreenRotationAnimatorForDisplay(display.id());
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  animator->Rotate(display::Display::ROTATE_90,
                   display::Display::RotationSource::USER,
                   DisplayConfigurationController::ANIMATION_SYNC);
}

// Verifies that rotating the screen when launcher is shown does not crash.
TEST_P(AppListBubbleAndTabletTest, RotationAnimationWithFolderSmoke) {
  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(1);
  model->CreateAndPopulateFolderWithApps(3);
  model->PopulateApps(1);

  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Tap the folder item to show it.
  GestureTapOn(apps_grid_view_->GetItemViewAt(1));
  ASSERT_TRUE(AppListIsInFolderView());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ScreenRotationAnimator* animator =
      DisplayConfigurationControllerTestApi(
          Shell::Get()->display_configuration_controller())
          .GetScreenRotationAnimatorForDisplay(display.id());
  animator->Rotate(display::Display::ROTATE_90,
                   display::Display::RotationSource::USER,
                   DisplayConfigurationController::ANIMATION_SYNC);

  // Close the folder view.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      GetFolderView()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();
  ASSERT_FALSE(AppListIsInFolderView());

  EXPECT_FALSE(GetFolderView()->shadow()->GetLayer()->visible());
}

TEST_P(AppListBubbleAndTabletTest, RotationAnimationInSearchSmoke) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();

  // Show search page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Add suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(CreateOmniboxSuggestionResult(
      "Another suggestion", /*support_removal=*/true));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId, /*support_removal=*/true));
  // The result list is updated asynchronously.
  base::RunLoop().RunUntilIdle();

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ScreenRotationAnimator* animator =
      DisplayConfigurationControllerTestApi(
          Shell::Get()->display_configuration_controller())
          .GetScreenRotationAnimatorForDisplay(display.id());
  animator->Rotate(display::Display::ROTATE_90,
                   display::Display::RotationSource::USER,
                   DisplayConfigurationController::ANIMATION_SYNC);
}

// Tests that mouse app list item drag is cancelled when mouse capture is lost
// (e.g. on screen rotation).
TEST_F(PopulatedAppListTest, CancelItemDragOnMouseCaptureLoss) {
  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPageInPagedGrid(0) + 1);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Start dragging the first item - move it in between items 1 and 2.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    event_generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());
    EXPECT_TRUE(apps_grid_view_->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    UpdateDisplay("600x1200");
    // AppListView is usually notified of display bounds changes by
    // AppListPresenter, though the test delegate implementation does not
    // track display metrics changes, so OnParentWindowBoundsChanged() has to be
    // explicitly called here.
    app_list_view_->OnParentWindowBoundsChanged();
    // Verify that mouse drag has been canceled due to mouse capture loss.
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
    EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
    EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

// Tests that app list item drag gets canceled if the dragged app list item gets
// deleted.
TEST_F(PopulatedAppListTest, CancelItemDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(4);

  // Start dragging a view.
  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    event_generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());
    EXPECT_TRUE(apps_grid_view_->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Delete the dragged item.
    GetAppListModel()->DeleteItem(dragged_view->item()->id());
    // Verify that mouse drag has been canceled.
    EXPECT_FALSE(apps_grid_view_->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Required by the DragDropController to finalize drag sequence.
    // TODO(b/261985897): Investigate the crash that occurs on these tests if
    // they are not properly releasing the drag.
    event_generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 3", apps_grid_view_->GetItemViewAt(2)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that app list item drag in folder gets canceled if the dragged app list
// item gets deleted.
TEST_F(PopulatedAppListTest, CancelFolderItemDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(3);

  // Tap the folder item to show it.
  GestureTapOn(apps_grid_view_->GetItemViewAt(2));
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    event_generator->MoveTouchBy(5, 5);
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Delete the dragged item.
    GetAppListModel()->DeleteItem(dragged_view->item()->id());
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Required by the DragDropController to finalize drag sequence.
    // TODO(b/261985897): Investigate the crash that occurs on these tests if
    // they are not properly releasing the drag.
    event_generator->ReleaseTouch();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 3",
            folder_view()->items_grid_view()->GetItemViewAt(0)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that app list item drag from folder to root apps grid gets canceled if
// the dragged app list item gets deleted.
TEST_F(PopulatedAppListTest, CancelFolderItemReparentDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(3);

  // Tap the folder item to show it.
  GestureTapOn(apps_grid_view_->GetItemViewAt(2));
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  const std::string dragged_app_id = dragged_view->item()->id();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Generate another mouse event to properly start the drag and drop sequence
    // with DragUpdate().
    event_generator->MoveTouchBy(5, 5);
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item outside the folder bounds and fire reparenting timer.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
    event_generator->MoveTouchBy(2, 2);
    EXPECT_FALSE(AppListIsInFolderView());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move item again to generate OnDragEnter()/OnDragExit() event.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());
    EXPECT_TRUE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Delete the dragged item.
    GetAppListModel()->DeleteItem(dragged_app_id);
    // Verify that drag has been canceled.
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Required by the DragDropController to finalize drag sequence.
    // TODO(b/261985897): Investigate the crash that occurs on these tests if
    // they are not properly releasing the drag.
    event_generator->ReleaseTouch();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 5", apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

TEST_F(PopulatedAppListTest,
       CancelFolderItemReparentDragOnDragItemAndFolderDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  CreateAndPopulateFolderWithApps(2);
  PopulateApps(3);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  const std::string dragged_app_id = dragged_view->item()->id();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    event_generator->MoveTouchBy(5, 5);
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item outside the folder bounds.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
    event_generator->MoveTouchBy(2, 2);

    EXPECT_FALSE(AppListIsInFolderView());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move item within the main grid.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());

    EXPECT_TRUE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Leave the dragged item as it's folder only child, and then delete it,
    // which should also delete the folder.
    GetAppListModel()->DeleteItem("Item 3");
    GetAppListModel()->DeleteItem(dragged_app_id);

    // Verify that drag has been canceled.
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Required by the DragDropController to finalize drag sequence.
    // TODO(b/261985897): Investigate the crash that occurs on these tests if
    // they are not properly releasing the drag.
    event_generator->ReleaseTouch();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 4", apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 5", apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that apps grid item layers are not destroyed immediately after item
// drag ends.
TEST_F(PopulatedAppListTest,
       ItemLayersNotDestroyedDuringBoundsAnimationAfterDrag) {
  InitializeAppsGrid();
  const int kItemCount = 5;
  PopulateApps(kItemCount);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Drag the first item between items 1 and 2.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    event_generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());

    // Items should have layers during app list item drag.
    for (int i = 0; i < kItemCount; ++i) {
      views::View* item_view = apps_grid_view_->view_model()->view_at(i);
      EXPECT_TRUE(item_view->layer()) << "at " << i;
    }

    EXPECT_TRUE(apps_grid_view_->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetEventGenerator()->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // After the drag is released, the item bounds should animate to their final
  // bounds.
  EXPECT_TRUE(apps_grid_view_->IsItemAnimationRunning());
  for (int i = 0; i < kItemCount; ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
  }

  // Wait for each item's layer animation to complete.
  ui::LayerAnimationStoppedWaiter animation_waiter;
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); i++) {
    if (apps_grid_view_->view_model()->view_at(i)->layer())
      animation_waiter.Wait(apps_grid_view_->view_model()->view_at(i)->layer());
  }

  // Layers should be destroyed once the item animations complete.
  for (int i = 0; i < kItemCount; ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests screen rotation during apps grid item drag where the drag gets
// canceled.
TEST_F(PopulatedAppListScreenRotationTest,
       ScreenRotationDuringAppsGridItemDragCancelsOperation) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPageInPagedGrid(0) +
               apps_grid_test_api_->TilesPerPageInPagedGrid(1));

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Start dragging the first item.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // While the drag is running, rotate screen.
    RotateScreen();
    EXPECT_FALSE(apps_grid_view_->IsDragging());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  // The model state should not have been changed.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
}

// Tests screen rotation during a folder apps grid item reparent drag where the
// drag gets canceled.
TEST_F(PopulatedAppListScreenRotationTest,
       ScreenRotationDuringFolderAppsGridItemDragCancelsOperation) {
  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(10);

  // Tap the folder item to show it.
  GestureTapOn(apps_grid_view_->GetItemViewAt(2));
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item within the folder bounds.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // While the drag is running, rotate screen.
    RotateScreen();
    EXPECT_FALSE(AppListIsInFolderView());
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  // The model state should not have been changed.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 5", apps_grid_view_->GetItemViewAt(3)->item()->id());
}

// Tests screen rotation during apps grid item drag where the drag gets
// canceled after a page change. Verifies the correct page is selected for the
// item that started the drag.
TEST_F(PopulatedAppListScreenRotationTest,
       ScreenRotationDuringAppsGridItemWithPageChange) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPageInPagedGrid(0) +
               apps_grid_test_api_->TilesPerPageInPagedGrid(1));

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_EQ(2, apps_grid_view_->pagination_model()->total_pages());

  // Start dragging the first item.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item close to apps grid edge, to flip to the next page.
    event_generator->MoveTouch(
        apps_grid_view_->GetBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, 5));
    EXPECT_TRUE(apps_grid_view_->FirePageFlipTimerForTest());
    apps_grid_view_->pagination_model()->FinishAnimation();
    EXPECT_EQ(1, apps_grid_view_->pagination_model()->selected_page());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // While the drag is running, rotate screen.
    RotateScreen();
    EXPECT_FALSE(AppListIsInFolderView());
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  // Make sure that the correct page displays, with the selected app.
  EXPECT_EQ(2, apps_grid_view_->pagination_model()->total_pages());
  EXPECT_EQ(0, apps_grid_view_->pagination_model()->selected_page());

  // The model state should not have been changed.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
}

// Tests screen rotation during apps grid item drag where the drag gets
// canceled after a page change. Verifies that the correct page is selected for
// them item that started the drag even if the item ends up in a different page.
TEST_F(PopulatedAppListScreenRotationTest,
       ScreenRotationDuringAppsGridItemWithPageNumberChange) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  // Initialize an apps grid with enough apps to have two pages on landscape
  // mode but only one page on portrait mode.
  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPageInPagedGrid(0) + 2);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);
  ASSERT_EQ(2, apps_grid_view_->pagination_model()->total_pages());

  // Start dragging the first item.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item close to apps grid edge, to flip to the next page.
    event_generator->MoveTouch(
        apps_grid_view_->GetBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, 5));
    EXPECT_TRUE(apps_grid_view_->FirePageFlipTimerForTest());
    apps_grid_view_->pagination_model()->FinishAnimation();
    EXPECT_EQ(1, apps_grid_view_->pagination_model()->selected_page());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // While the drag is running, rotate screen.
    RotateScreen();
    EXPECT_FALSE(AppListIsInFolderView());
    EXPECT_FALSE(apps_grid_view_->IsDragging());
    EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  // Make sure that the correct page displays, with the selected app.
  EXPECT_EQ(1, apps_grid_view_->pagination_model()->total_pages());
  EXPECT_EQ(0, apps_grid_view_->pagination_model()->selected_page());

  // The model state should not have been changed.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
}

// Tests that app list folder item reparenting drag to another folder.
TEST_P(AppListBubbleAndTabletDragTest, AppsGridItemReparentToFolderDrag) {
  UpdateDisplay("1200x600");

  test::AppListTestModel* model = GetAppListModel();
  model->PopulateApps(2);
  AppListFolderItem* folder = model->CreateAndPopulateFolderWithApps(3);
  model->PopulateApps(10);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Tap the folder item to show it.
  AppListItemView* folder_item = apps_grid_view_->GetItemViewAt(2);
  ASSERT_TRUE(folder_item);
  GestureTapOn(folder_item);
  ASSERT_TRUE(AppListIsInFolderView());
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Start dragging the first item in the active folder.
  AppListItemView* dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  ASSERT_TRUE(dragged_view);
  AppListItem* dragged_item = dragged_view->item();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item outside the folder bounds.
    event_generator->MoveTouch(
        apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
    event_generator->MoveTouchBy(2, 2);

    EXPECT_FALSE(AppListIsInFolderView());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the pointer over the item 3, and drop the dragged item.
    gfx::Point target =
        apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint();
    event_generator->MoveTouch(target);
    event_generator->ReleaseTouch();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  // Verify the new item location within the apps grid.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());

  EXPECT_TRUE(apps_grid_view_->GetItemViewAt(3)->item()->is_folder());
  EXPECT_EQ(dragged_item->folder_id(),
            apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Newly created folder should open and have the name input focused.
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(dragged_item->folder_id(), folder_view()->folder_item()->id());
  EXPECT_TRUE(folder_view()
                  ->folder_header_view()
                  ->GetFolderNameViewForTest()
                  ->HasFocus());
}

// Tests that an item can be removed just after creating a folder that contains
// that item. See https://crbug.com/1083942
TEST_F(PopulatedAppListTest, RemoveFolderItemAfterFolderCreation) {
  InitializeAppsGrid();
  const int kItemCount = 6;
  PopulateApps(kItemCount);

  // Dragging the item with index 2.
  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(2);
  AppListItem* const dragged_item = dragged_view->item();
  AppListItem* const merged_item = apps_grid_view_->GetItemViewAt(3)->item();

  const gfx::Rect expected_folder_item_view_bounds =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen();

  // Drag the item on top of the item with index 3.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move mouse to switch to cardified state -the cardified state starts only
    // once the drag distance exceeds a drag threshold, so the pointer has to
    // sufficiently move from the original position.
    event_generator->MoveMouseBy(10, 10);
    event_generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());
    event_generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_FALSE(apps_grid_view_->IsDragging());

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(2);
  EXPECT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(expected_folder_item_view_bounds,
            folder_item_view->GetBoundsInScreen());
  EXPECT_EQ(dragged_item->folder_id(), folder_item_view->item()->id());

  // Verify that item layers have been destroyed after the drag operation ended.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Verify that item views have no layers after the folder has been opened.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_EQ(expected_folder_item_view_bounds,
            folder_item_view->GetBoundsInScreen());
  EXPECT_TRUE(AppListIsInFolderView());
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Remove an item from the folder, and leave it as a single item folder.
  GetAppListModel()->DeleteItem(merged_item->id());
  EXPECT_TRUE(AppListIsInFolderView());
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Remove the original drag view item.
  GetAppListModel()->DeleteItem(dragged_item->id());
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  EXPECT_FALSE(AppListIsInFolderView());
  EXPECT_FALSE(apps_grid_view_->GetItemViewAt(2)->item()->is_folder());

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
}

TEST_F(PopulatedAppListTest, ReparentLastFolderItemAfterFolderCreation) {
  InitializeAppsGrid();
  const int kItemCount = 5;
  PopulateApps(kItemCount);

  // Dragging the item with index 4.
  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(4);
  AppListItem* const dragged_item = dragged_view->item();
  AppListItem* const merged_item = apps_grid_view_->GetItemViewAt(3)->item();
  const gfx::Rect expected_folder_item_view_bounds =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen();

  // Drag the item on top of the item with index 3.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move mouse to switch to cardified state -the cardified state starts only
    // once the drag distance exceeds a drag threshold, so the pointer has to
    // sufficiently move from the original position.
    event_generator->MoveMouseBy(10, 10);
    event_generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());
    event_generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  EXPECT_FALSE(apps_grid_view_->IsDragging());

  AppListItem* folder_item = apps_grid_view_->GetItemViewAt(3)->item();
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(dragged_item->folder_id(), folder_item->id());

  // Verify that item layers have been destroyed after the drag operation ended.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  AppListItemView* const folder_item_view = apps_grid_view_->GetItemViewAt(3);
  EXPECT_TRUE(folder_item_view->is_folder());
  EXPECT_EQ(expected_folder_item_view_bounds,
            folder_item_view->GetBoundsInScreen());

  // Verify that item views have no layers after the folder has been opened.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_TRUE(AppListIsInFolderView());
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Remove the original drag view item.
  GetAppListModel()->DeleteItem(dragged_item->id());
  // Reparent the remaining folder item to the root apps grid (as it's done by
  // Chrome when cleaning up single-item folders).
  GetAppListModel()->MoveItemToRootAt(merged_item, folder_item->position());
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  EXPECT_FALSE(AppListIsInFolderView());
  EXPECT_FALSE(apps_grid_view_->GetItemViewAt(3)->item()->is_folder());

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
}

TEST_F(PopulatedAppListWithVKEnabledTest,
       TappingAppsGridClosesVirtualKeyboard) {
  InitializeAppsGrid();
  PopulateApps(2);
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  views::View::ConvertPointToScreen(apps_grid_view_, &between_apps);
  gfx::Point empty_space = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
  views::View::ConvertPointToScreen(apps_grid_view_, &empty_space);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Touch the apps_grid outside of any apps. Expect that the keyboard is
  // closed.
  GetEventGenerator()->GestureTapAt(empty_space);
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // Reshow the VKeyboard
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Touch the apps_grid between two apps. Expect that the keyboard is closed.
  GetEventGenerator()->GestureTapAt(between_apps);
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Tests that app list hides when focus moves to a normal window.
TEST_P(AppListPresenterTest, HideOnFocusOut) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_P(AppListPresenterTest, RemainVisibleWhenFocusingToApplistContainer) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests opening the app list on a secondary display, then deleting the display.
TEST_P(AppListPresenterTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ("1024,0 1024x768", root_windows[1]->GetBoundsInScreen().ToString());

  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests updating display should not close the app list.
TEST_P(AppListPresenterTest, UpdateDisplayNotCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Change display bounds.
  UpdateDisplay("1024x768");

  // Updating the display should not close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the app list window's bounds and the search box bounds are updated
// when the display bounds change.
TEST_F(AppListPresenterTest, AppListBoundsChangeForDisplayChange) {
  UpdateDisplay("1024x768");
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetSearchBoxView()->GetBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetSearchBoxView()->GetBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen state are updated when the display bounds change.
TEST_F(AppListPresenterTest, AppListBoundsChangeForDisplayChangeFullscreen) {
  EnableTabletMode(true);
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetSearchBoxView()->GetBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetSearchBoxView()->GetBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen search state are updated when the display bounds change.
TEST_F(AppListPresenterTest,
       AppListBoundsChangeForDisplayChangeFullscreenSearch) {
  EnableTabletMode(true);
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);
  SetAppListStateAndWait(AppListViewState::kFullscreenSearch);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetSearchBoxView()->GetBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetSearchBoxView()->GetBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list initializes in fullscreen with tablet mode active
// and that the state transitions via text input act properly.
TEST_P(AppListPresenterTest, TabletModeTextStateTransitions) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Pressing the escape key should transition the app list to the fullscreen
  // all apps state.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list closes when tablet mode deactivates.
TEST_P(AppListPresenterTest, AppListClosesWhenLeavingTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Tests the shelf background type is as expected when a window is created after
// going to tablet mode.
TEST_F(AppListPresenterTest, ShelfBackgroundWithHomeLauncher) {
  // Enter tablet mode to display the home launcher.
  EnableTabletMode(true);
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            shelf_layout_manager->shelf_background_type());

  // Add a window. It should be in-app because it is in tablet mode.
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            shelf_layout_manager->shelf_background_type());
}

// Tests that the bottom shelf is auto hidden when a window is fullscreened in
// tablet mode (home launcher is shown behind).
TEST_F(AppListPresenterTest, ShelfAutoHiddenWhenFullscreen) {
  EnableTabletMode(true);
  Shelf* shelf =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()));
  EXPECT_EQ(ShelfVisibilityState::SHELF_VISIBLE, shelf->GetVisibilityState());

  // Create and fullscreen a window. The shelf should be auto hidden.
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(ShelfVisibilityState::SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            shelf->GetAutoHideState());
}

// Tests that a keypress activates the searchbox and that clearing the
// searchbox, the searchbox remains active. Does not apply to bubble launcher,
// where the search box is always active.
TEST_F(AppListPresenterTest, KeyPressEnablesSearchBox) {
  EnableTabletMode(true);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Press any key, the search box should be active.
  PressAndReleaseKey(ui::VKEY_0);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Delete the text, the search box should be inactive.
  search_box_view->ClearSearch();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

// Tests that search box gets deactivated if the active search model gets
// switched. Does not apply to bubble launcher, where the search box is
// always active.
TEST_P(AppListPresenterTest, SearchBoxDeactivatedOnModelChange) {
  EnableTabletMode(true);

  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Switch the active app list and search model, and verify the search box is
  // deactivated.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model_override =
      std::make_unique<QuickAppAccessModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model_override.get());

  EXPECT_FALSE(search_box_view->is_search_box_active());

  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  Shell::Get()->app_list_controller()->ClearActiveModel();
}

// Tests that search UI gets closed if search model changes.
TEST_F(AppListPresenterTest, SearchClearedOnModelChange) {
  EnableTabletMode(true);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Press a key to start search, and activate the search box.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel* search_model = GetSearchModel();
  auto test_result = std::make_unique<TestSearchResult>();
  test_result->set_result_id("test");
  // Give this item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  test_result->SetAccessibleName(u"test");
  test_result->set_display_type(SearchResultDisplayType::kList);
  search_model->results()->Add(std::move(test_result));

  auto test_list_result = std::make_unique<TestSearchResult>();
  test_list_result->set_result_id("test_list");
  // Give this item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  test_list_result->SetAccessibleName(u"test_list");
  test_list_result->set_best_match(true);
  test_list_result->set_display_type(SearchResultDisplayType::kList);
  search_model->results()->Add(std::move(test_list_result));

  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  SearchResultContainerView* item_list_container =
      GetDefaultSearchResultListView();
  ASSERT_EQ(1u, item_list_container->num_results());
  EXPECT_EQ("test_list",
            item_list_container->GetResultViewAt(0)->result()->id());

  // Switch the active app list and search model, and verify the search UI gets
  // cleared.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model_override =
      std::make_unique<QuickAppAccessModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model_override.get());

  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Verify that the search UI shows results from the current active search
  // model.
  PressAndReleaseKey(ui::VKEY_A);

  auto test_list_result_override = std::make_unique<TestSearchResult>();
  test_list_result_override->set_result_id("test_list_override");
  // Give this item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  test_list_result_override->SetAccessibleName(u"test_list_override");
  test_list_result_override->set_display_type(SearchResultDisplayType::kList);
  test_list_result_override->set_best_match(true);
  search_model_override->results()->Add(std::move(test_list_result_override));

  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  item_list_container = GetDefaultSearchResultListView();
  ASSERT_EQ(1u, item_list_container->num_results());
  EXPECT_EQ("test_list_override",
            item_list_container->GetResultViewAt(0)->result()->id());

  Shell::Get()->app_list_controller()->ClearActiveModel();

  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the result selection will reset after closing the search box by
// clicking the close button.
TEST_F(AppListTabletTest,
       ClosingSearchBoxByClickingCloseButtonResetsResultSelection) {
  EnableTabletMode(true);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  SearchBoxView* search_box_view = GetSearchBoxView();
  ResultSelectionController* result_selection_controller =
      GetResultSelectionController();

  // Add search results to the search model.
  // Click on the search box to activate search.
  GetEventGenerator()->GestureTapAt(SearchBoxCenterPoint());
  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_FALSE(result_selection_controller->selected_result());

  // Start search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_A, 0);

  // Add some search results to the search model.
  SearchModel* search_model = GetSearchModel();
  search_model->results()->Add(
      CreateOmniboxSuggestionResult("Suggestion1", /*support_removal=*/true));
  search_model->results()->Add(
      CreateOmniboxSuggestionResult("Suggestion2", /*support_removal=*/true));

  EXPECT_TRUE(AppListSearchResultPageVisible());

  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  // Click the search box, the result selection should be the first one in
  // default.
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_TRUE(result_selection_controller->selected_location_details()
                  ->is_first_result());

  // Move the selection to the second result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_FALSE(result_selection_controller->selected_location_details()
                   ->is_first_result());

  // Use the close button in search_box_view to close the search box.
  const views::View* close_button = GetSearchBoxView()->close_button();
  GestureTapOn(close_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Delete all search result after closing the search box.
  search_model->DeleteAllResults();

  // Tap the search box again, the result selection should be reset to the first
  // one.
  GestureTapOn(search_box_view);

  // Verify that there is no existing result.
  EXPECT_TRUE(search_box_view->is_search_box_active());
  EXPECT_FALSE(result_selection_controller->selected_result());
}

// Regression test for crash due to use-after-free. https://crbug.com/1163332
TEST_P(AppListPresenterTest, ShouldNotCrashOnItemClickAfterMonitorDisconnect) {
  // Set up two displays.
  UpdateDisplay("1024x768,1200x900");
  test::AppListTestModel* model = GetAppListModel();
  AppListItem* item0 = model->AddItem(new AppListItem("item 0"));
  AppListItem* item1 = model->AddItem(new AppListItem("item 1"));

  // Give each item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  model->SetItemName(item0, item0->id());
  model->SetItemName(item1, item1->id());

  // Open and close app list on secondary display.
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetSecondaryDisplay().id());
  helper->DismissAndRunLoop();

  // Open and close app list on primary display.
  helper->ShowAndRunLoop(GetPrimaryDisplayId());
  helper->DismissAndRunLoop();

  // Disconnect secondary display.
  UpdateDisplay("1024x768");

  // Open app list to show app items.
  EnsureLauncherWithVisibleAppsGrid();

  // Click on an item.
  AppListItemView* item_view = apps_grid_view()->GetItemViewAt(0);
  EXPECT_EQ(item_view->GetViewAccessibility().GetCachedName(),
            base::UTF8ToUTF16(item0->id()));
  LeftClickOn(item_view);

  // No crash. No use-after-free detected by ASAN.
}

// Tests that no crash occurs after an attempt to show app list in an invalid
// display.
TEST_P(AppListPresenterTest, ShowInInvalidDisplay) {
  GetAppListTestHelper()->ShowAndRunLoop(display::kInvalidDisplayId);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

TEST_F(AppListPresenterTest, TapAppListThenSystemTrayShowsAutoHiddenShelf) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a normal unmaximized window; the shelf should be hidden.
  std::unique_ptr<views::Widget> window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap the system tray should open system tray bubble and keep shelf visible.
  GestureTapOn(GetPrimaryUnifiedSystemTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap to dismiss the app list and the auto-hide shelf.
  GetEventGenerator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListPresenterTest, TapAppListThenShelfHidesAutoHiddenShelf) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a normal unmaximized window; the shelf should be hidden.
  std::unique_ptr<views::Widget> window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the AppList.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Make sure the shelf has at least one item.
  ShelfItem item =
      ShelfTestUtil::AddAppShortcut(base::NumberToString(1), TYPE_PINNED_APP);

  // Wait for shelf view's bounds animation to end. Otherwise the scrollable
  // shelf's bounds are not updated yet.
  ShelfView* const shelf_view = shelf->GetShelfViewForTesting();
  ShelfViewTestAPI shelf_view_test_api(shelf_view);
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  // Test that tapping the auto-hidden shelf dismisses the app list when tapping
  // part of the shelf that does not contain the apps.
  GetEventGenerator()->GestureTapAt(
      shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(10, 0));
  base::RunLoop().RunUntilIdle();  // Wait for autohide to be recomputed.
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the AppList again.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // App list should remain visible when tapping on a shelf app button.
  ASSERT_TRUE(shelf_view_test_api.GetButton(0));
  GestureTapOn(shelf_view_test_api.GetButton(0));
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

TEST_F(AppListPresenterTest, ClickingShelfArrowDoesNotHideAppList) {
  SetShelfAnimationDuration(base::Milliseconds(1));

  // Add enough shelf items for the shelf to enter overflow.
  Shelf* const shelf = GetPrimaryShelf();
  ScrollableShelfView* const scrollable_shelf_view =
      shelf->hotseat_widget()->scrollable_shelf_view();
  int index = 0;
  while (scrollable_shelf_view->layout_strategy_for_test() ==
         ScrollableShelfView::kNotShowArrowButtons) {
    ShelfItem item = ShelfTestUtil::AddAppShortcut(
        base::NumberToString(index++), TYPE_PINNED_APP);
  }

  WaitForShelfAnimation();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click right scrollable shelf arrow - verify the the app list remains
  // visible.
  const views::View* right_arrow = scrollable_shelf_view->right_arrow();
  ASSERT_TRUE(right_arrow->GetVisible());
  LeftClickOn(right_arrow);

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click left button - verify the app list stays visible.
  const views::View* left_arrow = scrollable_shelf_view->left_arrow();
  ASSERT_TRUE(left_arrow->GetVisible());
  LeftClickOn(left_arrow);

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click right of the right arrow - verify the app list gets dismissed.
  ASSERT_TRUE(right_arrow->GetVisible());
  GetEventGenerator()->MoveMouseTo(
      right_arrow->GetBoundsInScreen().right_center() + gfx::Vector2d(10, 0));
  GetEventGenerator()->ClickLeftButton();

  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Tests that the touch selection menu created when tapping an open folder's
// folder name view be interacted with.
TEST_F(PopulatedAppListTest, TouchSelectionMenu) {
  InitializeAppsGrid();

  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(4);
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(1u, GetAppListModel()->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType, GetAppListTestHelper()
                                              ->model()
                                              ->top_level_item_list()
                                              ->item_at(0)
                                              ->GetItemType());

  // Open the folder.
  ASSERT_FALSE(AppListIsInFolderView());
  GestureTapOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(AppListIsInFolderView());

  // Check that the touch selection menu runner is not running.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Set the folder name and simulate tap on the folder name view.
  views::View* folder_name_view =
      folder_view()->folder_header_view()->GetFolderNameViewForTest();
  UpdateFolderName("folder_name");
  GestureTapOn(folder_name_view);

  // Fire the timer to show the textfield quick menu.
  views::TextfieldTestApi textfield_test_api(
      folder_view()->folder_header_view()->GetFolderNameViewForTest());
  static_cast<views::TouchSelectionControllerImpl*>(
      textfield_test_api.touch_selection_controller())
      ->ShowQuickMenuImmediatelyForTesting();
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the leftmost button in the touch_menu_container and check that the
  // folder name has been cut.
  views::TouchSelectionMenuRunnerViews::TestApi test_api(
      static_cast<views::TouchSelectionMenuRunnerViews*>(
          ui::TouchSelectionMenuRunner::GetInstance()));
  views::LabelButton* button = test_api.GetFirstButton();
  ASSERT_TRUE(button);
  GestureTapOn(button);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Refresh the folder item name.
  RefreshFolderName();
  // Check folder_name_view's name.
  ASSERT_EQ("", GetFolderName());
}

// Test a variety of behaviors for home launcher (app list in tablet mode).
// Parameterized by Mouse or touch parameter.
class AppListPresenterHomeLauncherTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AppListPresenterHomeLauncherTest() = default;
  AppListPresenterHomeLauncherTest(const AppListPresenterHomeLauncherTest&) =
      delete;
  AppListPresenterHomeLauncherTest& operator=(
      const AppListPresenterHomeLauncherTest&) = delete;
  ~AppListPresenterHomeLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    GetAppListTestHelper()->WaitUntilIdle();
    wallpaper_test_api_ = std::make_unique<WallpaperControllerTestApi>(
        Shell::Get()->wallpaper_controller());
  }

  void TearDown() override {
    wallpaper_test_api_.reset();
    AshTestBase::TearDown();
  }

  bool TestMouseEventParam() const { return GetParam(); }

  void TapHomeButton(int64_t display_id) {
    HomeButton* const home_button =
        Shell::GetRootWindowControllerWithDisplayId(display_id)
            ->shelf()
            ->navigation_widget()
            ->GetHomeButton();
    gfx::Point tap_point = home_button->GetBoundsInScreen().CenterPoint();
    GetEventGenerator()->GestureTapDownAndUp(tap_point);
    GetAppListTestHelper()->WaitUntilIdle();
  }

  // Ensures transition to home screen in tablet mode (where home button is not
  // always shown).
  void GoHome() {
    const int64_t primary_display_id = GetPrimaryDisplay().id();
    // If home button is not expected to be shown, use
    // AppListControllerImpl::GoHome() directly, otherwise tap on the primary
    // screen home button.
    if (!Shell::Get()->shelf_config()->shelf_controls_shown()) {
      Shell::Get()->app_list_controller()->GoHome(primary_display_id);
      return;
    }
    TapHomeButton(primary_display_id);
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

  gfx::Point GetPointOutsideSearchbox() {
    // Ensures that the point satisfies the following conditions:
    // (1) The point is within AppListView.
    // (2) The point is outside of the search box.
    // (3) The touch event on the point should not be consumed by the handler
    // for back gesture.
    return GetSearchBoxViewFromHelper(GetAppListTestHelper())
        ->GetBoundsInScreen()
        .bottom_right();
  }

  gfx::Point GetPointInsideSearchbox() {
    return GetSearchBoxViewFromHelper(GetAppListTestHelper())
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  void ShowAppList() {
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  }

  bool IsAppListVisible() {
    auto* app_list_controller = Shell::Get()->app_list_controller();
    return app_list_controller->IsVisible() &&
           app_list_controller->GetTargetVisibility(std::nullopt);
  }

 protected:
  std::unique_ptr<WallpaperControllerTestApi> wallpaper_test_api_;
};

INSTANTIATE_TEST_SUITE_P(MouseTouchEvent,
                         AppListPresenterHomeLauncherTest,
                         testing::Bool());

// Tests that the app list is shown automatically when the tablet mode is on.
// The app list is dismissed when the tablet mode is off.
TEST_F(AppListPresenterHomeLauncherTest, ShowAppListForTabletMode) {
  GetAppListTestHelper()->CheckVisibility(false);

  // Turns on tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Turns off tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListPresenterHomeLauncherTest,
       RunZeroStateSearchWhenShownOnTabletModeTransition) {
  EXPECT_EQ(0, GetTestAppListClient()->start_zero_state_search_count());
  GetAppListTestHelper()->CheckVisibility(false);

  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());
}

TEST_F(AppListPresenterHomeLauncherTest,
       RunZeroStateSearchWhenShownAfterMinimizingWindows) {
  EXPECT_EQ(0, GetTestAppListClient()->start_zero_state_search_count());
  GetAppListTestHelper()->CheckVisibility(false);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(0, GetTestAppListClient()->start_zero_state_search_count());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMinimized);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());
}

// Tests that the app list window's parent is changed after entering tablet
// mode.
TEST_F(AppListPresenterHomeLauncherTest, ParentWindowContainer) {
  // Show app list in non-tablet mode. The window container should be
  // kShellWindowId_AppListContainer.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  aura::Window* window = Shell::Get()->app_list_controller()->GetWindow();
  aura::Window* root_window = window->GetRootWindow();
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_AppListContainer)
                  ->Contains(window));

  // Turn on tablet mode. The window container should be
  // kShellWindowId_HomeScreenContainer.
  EnableTabletMode(true);
  aura::Window* window2 = Shell::Get()->app_list_controller()->GetWindow();
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_HomeScreenContainer)
                  ->Contains(window2));
}

// Tests that tapping or clicking on background cannot dismiss the app list.
TEST_F(AppListPresenterHomeLauncherTest, TapOrClickToDismiss) {
  // Show app list in non-tablet mode. Click outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Point origin;
  generator->MoveMouseTo(origin);
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(IsAppListVisible());

  // Show app list in non-tablet mode. Tap outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->GestureTapDownAndUp(origin);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(IsAppListVisible());

  // Show app list in tablet mode. Click outside search box.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(origin);
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(IsAppListVisible());

  // Tap outside search box.
  generator->GestureTapDownAndUp(origin);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest,
       EscapeKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest, BackKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest,
       SearchKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest,
       EscapeKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest,
       BackKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

TEST_F(AppListPresenterHomeLauncherTest,
       SearchKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

// Tests that moving focus outside app list window can dismiss it.
TEST_F(AppListPresenterHomeLauncherTest, FocusOutToDismiss) {
  // Show app list in non-tablet mode. Move focus to another window.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Go to tablet mode with a focused window, the AppList should not be visible.
  EnableTabletMode(true);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Refocusing the already focused window should change nothing.
  wm::ActivateWindow(window.get());

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Minimizing the focused window with no remaining windows should result in a
  // shown applist.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMinimized);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that tapping home button while home screen is visible and showing
// search results moves the home screen to apps container page.
TEST_F(AppListPresenterHomeLauncherTest, HomeButtonDismissesSearchResults) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enable accessibility feature that forces home button to be shown even with
  // kHideShelfControlsInTabletMode enabled.
  // TODO(crbug.com/40673209) Use the a11y feature specific to showing
  // navigation buttons in tablet mode once it lands.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Tap home button - verify that home goes back to showing the apps page.
  TapHomeButton(GetPrimaryDisplay().id());

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests the app list opacity in overview mode.
TEST_F(AppListPresenterHomeLauncherTest, OpacityInOverviewMode) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ui::Layer* layer = GetAppListView()->GetWidget()->GetNativeWindow()->layer();
  EXPECT_EQ(0.0f, layer->opacity());

  // Disable overview mode.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1.0f, layer->opacity());
}

TEST_F(AppListPresenterHomeLauncherTest, AppListHiddenDuringWallpaperPreview) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewConfirmed) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  wallpaper_test_api_->EndWallpaperPreview(/*confirm_preview_wallpaper=*/true);
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_F(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewCanceled) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  wallpaper_test_api_->EndWallpaperPreview(/*confirm_preview_wallpaper=*/false);
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_F(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewAndExitOverviewMode) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  EnterOverview();
  EXPECT_FALSE(IsAppListVisible());

  // Disable overview mode.
  ExitOverview();
  EXPECT_TRUE(IsAppListVisible());
}

// Tests that going home will minimize all windows.
TEST_F(AppListPresenterHomeLauncherTest, GoingHomeMinimizesAllWindows) {
  // Show app list in tablet mode. Maximize all windows.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0)),
      window2(CreateTestWindowInShellWithId(1)),
      window3(CreateTestWindowInShellWithId(2));
  WindowState* state1 = WindowState::Get(window1.get());
  WindowState* state2 = WindowState::Get(window2.get());
  WindowState* state3 = WindowState::Get(window3.get());
  state1->Maximize();
  state2->Maximize();
  state3->Maximize();
  EXPECT_TRUE(state1->IsMaximized());
  EXPECT_TRUE(state2->IsMaximized());
  EXPECT_TRUE(state3->IsMaximized());

  // The windows need to be activated for the mru window tracker.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  auto ordering =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  GoHome();
  EXPECT_TRUE(state1->IsMinimized());
  EXPECT_TRUE(state2->IsMinimized());
  EXPECT_TRUE(state3->IsMinimized());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tests that the window ordering remains the same as before we minimize.
  auto new_order =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  EXPECT_TRUE(base::ranges::equal(ordering, new_order));
}

// Tests that going home will end split view mode.
TEST_F(AppListPresenterHomeLauncherTest, GoingHomeEndsSplitViewMode) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  GoHome();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that going home will end overview mode.
TEST_F(AppListPresenterHomeLauncherTest, GoingHomeEndOverviewMode) {
  // Show app list in tablet mode. Enter overview mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  GoHome();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that going home will end overview and split view mode if both are
// active (e.g. one side of the split view contains overview).
TEST_F(AppListPresenterHomeLauncherTest,
       GoingHomeEndsSplitViewModeWithOverview) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> dummy_window(CreateTestWindowInShellWithId(1));

  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  GoHome();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the context menu is triggered in the same way as if we are on
// the wallpaper.
TEST_F(AppListPresenterHomeLauncherTest, WallpaperContextMenu) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Long press on the app list to open the context menu.
  // TODO(ginko) look into a way to populate an apps grid, then get a point
  // between these apps so that clicks/taps between apps can be tested
  const gfx::Point onscreen_point(GetPointOutsideSearchbox());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ui::GestureEvent long_press(
      onscreen_point.x(), onscreen_point.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  const aura::Window* root = window_util::GetRootWindowAt(onscreen_point);
  const RootWindowController* root_window_controller =
      RootWindowController::ForWindow(root);
  EXPECT_TRUE(root_window_controller->IsContextMenuShownForTest());

  // Tap down to close the context menu.
  ui::GestureEvent tap_down(
      onscreen_point.x(), onscreen_point.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  generator->Dispatch(&tap_down);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShownForTest());

  // Right click to open the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickRightButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(root_window_controller->IsContextMenuShownForTest());

  // Left click to close the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShownForTest());
}

// Test backdrop exists for active non-fullscreen window in tablet mode.
TEST_F(AppListPresenterHomeLauncherTest, BackdropTest) {
  WorkspaceControllerTestApi test_helper(ShellTestApi().workspace_controller());
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(test_helper.GetBackdropWindow());

  std::unique_ptr<aura::Window> non_fullscreen_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  non_fullscreen_window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_TRUE(test_helper.GetBackdropWindow());
}

// Tests that app list is not active when switching to tablet mode if an active
// window exists.
TEST_F(AppListPresenterHomeLauncherTest,
       NotActivateAppListWindowWhenActiveWindowExists) {
  // No window is active.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Show app list in tablet mode. It should become active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(GetAppListView()->GetWidget()->GetNativeWindow(),
            window_util::GetActiveWindow());

  // End tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate a window.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState::Get(window.get())->Activate();
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Show app list in tablet mode. It should not be active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Tests that involve the virtual keyboard.
class AppListPresenterVirtualKeyboardTest : public AppListPresenterTest {
 public:
  AppListPresenterVirtualKeyboardTest() = default;
  ~AppListPresenterVirtualKeyboardTest() override = default;

  // AppListPresenterTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AppListPresenterTest::SetUp();
  }

  // Performs mouse click or tap gesture on the provided point, depending on
  // whether the test is parameterized to use mouse clicks or tap gestures.
  void ClickOrTap(const gfx::Point& point) {
    if (TestMouseEventParam())
      ClickMouseAt(point);
    else
      GetEventGenerator()->GestureTapAt(point);
  }
};

// Instantiate the values in the parameterized tests. Used to toggle mouse and
// touch events.
INSTANTIATE_TEST_SUITE_P(Mouse,
                         AppListPresenterVirtualKeyboardTest,
                         testing::Bool());

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard when there exists text in the searchbox results in the virtual
// keyboard closing with no side effects.
TEST_P(AppListPresenterVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboardWithSearchText) {
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ClickOrTap(GetPointInsideSearchbox());

  // Enter some text in the searchbox, the applist should transition to
  // fullscreen search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide.
  ClickOrTap(GetPointOutsideSearchbox());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // The searchbox should still be active and the AppListView should still be in
  // FULLSCREEN_SEARCH.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(GetSearchBoxView()->is_search_box_active());

  // Tap or click the body of the AppList again, the searchbox should deactivate
  // and the applist should be in FULLSCREEN_ALL_APPS.
  ClickOrTap(GetPointOutsideSearchbox());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(GetSearchBoxView()->is_search_box_active());
}

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard when there is no text in the searchbox results in both the virtual
// keyboard and searchbox closing with no side effects.
TEST_P(AppListPresenterVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboardWithoutSearchText) {
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ClickOrTap(GetPointInsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide and
  // the searchbox should be inactive when there is no text in the searchbox.
  ClickOrTap(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  EXPECT_FALSE(GetSearchBoxView()->is_search_box_active());
}

TEST_F(AppListPresenterHomeLauncherTest, TapHomeButtonOnExternalDisplay) {
  UpdateDisplay("800x600,1000x768");

  TapHomeButton(GetSecondaryDisplay().id());
  {
    SCOPED_TRACE("1st tap");
    GetAppListTestHelper()->CheckVisibility(true);
  }

  TapHomeButton(GetSecondaryDisplay().id());
  {
    SCOPED_TRACE("2nd tap");
    GetAppListTestHelper()->CheckVisibility(false);
  }
}

// Tests that a tap/click on the AppListView from Fullscreen search returns
// the AppListView to fullscreen all apps, and that a tap/click on the
// AppListView from fullscreen all apps closes the app list.
TEST_P(AppListPresenterHomeLauncherTest,
       StateTransitionsByTappingAppListBodyFromFullscreen) {
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();
  const bool test_mouse_event = TestMouseEventParam();

  // Press a key, this should activate the searchbox and transition to
  // fullscreen search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the searchbox, this should deactivate the searchbox and the
  // applistview should return to fullscreen all apps.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(search_box_view->is_search_box_active());
}

// Tests that the searchbox activates when it is tapped and that the widget is
// closed after tapping outside the searchbox.
TEST_P(AppListPresenterHomeLauncherTest, TapAndClickEnablesSearchBox) {
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();
  const bool test_mouse_event = TestMouseEventParam();

  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Test that gesture tapping the app list search box correctly handles the event
// by moving the textfield's cursor to the tapped position within the text.
TEST_P(AppListPresenterTest, SearchBoxTextfieldGestureTap) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Tap on the search box to focus and open it.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Set the text of the search box textfield.
  views::Textfield* textfield = GetSearchBoxView()->search_box();
  textfield->SetText(u"Test search box string");

  // The textfield's cursor position should start out at the end of the string.
  size_t initial_cursor_position = textfield->GetCursorPosition();

  // Gesture tap to move the cursor to the middle of the string.
  gfx::Rect textfield_bounds = textfield->GetBoundsInScreen();
  gfx::Rect cursor_bounds =
      views::TextfieldTestApi(textfield).GetCursorViewRect();
  EXPECT_GT(cursor_bounds.x(), 0);
  gfx::Point touch_location(textfield_bounds.x() + cursor_bounds.x() / 2,
                            textfield_bounds.y());
  generator->GestureTapAt(touch_location);

  // Cursor position should have changed after the gesture tap.
  EXPECT_LT(textfield->GetCursorPosition(), initial_cursor_position);
}

// Tests tablet <-> clamshell mode transition.
class AppListPresenterWithScaleAnimationOnTabletModeTransitionTest
    : public AppListPresenterTest {
 protected:
  void EnsureAppListViewIsCached() {
    ASSERT_FALSE(GetAppListTestHelper()->GetAppListView());

    EnableTabletMode(true);
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

    EnableTabletMode(false);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

    // Entering and exiting from tablet mode should keep `AppListView` cached.
    ASSERT_TRUE(GetAppListTestHelper()->GetAppListView());
  }
};

TEST_F(AppListPresenterWithScaleAnimationOnTabletModeTransitionTest,
       UpdatesScaleAndOpacity) {
  // Enter tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  auto* const window =
      GetAppListTestHelper()->GetAppListView()->GetWidget()->GetNativeWindow();
  auto* const layer = window->layer();
  const auto center_point = gfx::Rect(layer->size()).CenterPoint();
  const auto no_transform = gfx::GetScaleTransform(center_point, 1.0f);
  const auto scaled_down_transform =
      gfx::GetScaleTransform(center_point, 0.92f);

  // The layer is fully visible and without applied transform.
  EXPECT_EQ(layer->opacity(), 1.0f);
  EXPECT_EQ(layer->transform(), no_transform);
  EXPECT_TRUE(window->IsVisible());

  // Exit tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  // The layer is fully transparent, scaled down and the window is hidden.
  EXPECT_EQ(layer->opacity(), 0.00f);
  EXPECT_EQ(layer->transform(), scaled_down_transform);
  EXPECT_FALSE(window->IsVisible());
}

TEST_F(AppListPresenterWithScaleAnimationOnTabletModeTransitionTest,
       ExitingFromOverviewInClamshellModeShouldNotAffectFullscreenLauncher) {
  EnsureAppListViewIsCached();

  auto* const layer = GetAppListTestHelper()
                          ->GetAppListView()
                          ->GetWidget()
                          ->GetNativeWindow()
                          ->layer();
  const auto expected_opacity = layer->opacity();
  const auto expected_transform = layer->transform();

  OverviewController* overview_controller = OverviewController::Get();

  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Layer's opacity and transform should stay the same.
  EXPECT_EQ(layer->opacity(), expected_opacity);
  EXPECT_EQ(layer->transform(), expected_transform);
}

// Tests that dismiss animation while animating to fullscreen state and open
// animation while animating to closed state continue from the same
// opacity/scale values where it was interrupted.
TEST_F(AppListPresenterWithScaleAnimationOnTabletModeTransitionTest,
       TransitionContinuesWhereItWasInterrupted) {
  EnsureAppListViewIsCached();
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* const layer = GetAppListTestHelper()
                          ->GetAppListView()
                          ->GetWidget()
                          ->GetNativeWindow()
                          ->layer();
  const auto center_point = gfx::Rect(layer->size()).CenterPoint();
  const auto no_transform = gfx::GetScaleTransform(center_point, 1.0f);
  const auto scaled_down_transform =
      gfx::GetScaleTransform(center_point, 0.92f);

  const auto initial_opacity = 0.01f;
  const auto initial_transform = scaled_down_transform;

  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(layer->opacity(), initial_opacity);
  EXPECT_EQ(layer->GetTargetOpacity(), 1.0f);
  EXPECT_EQ(layer->transform(), initial_transform);
  EXPECT_EQ(layer->GetTargetTransform(), no_transform);

  // Interrupt clamshell -> tablet transition by switching back to clamshell
  // mode. Current transform and opacity stay at initial values.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(layer->opacity(), initial_opacity);
  EXPECT_EQ(layer->GetTargetOpacity(), 0.0f);
  EXPECT_EQ(layer->transform(), initial_transform);
  EXPECT_EQ(layer->GetTargetTransform(), scaled_down_transform);

  // Interrupt tablet -> clamshell transition by switching back to tablet mode.
  // Current transform and opacity stay at initial values.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(layer->opacity(), initial_opacity);
  EXPECT_EQ(layer->GetTargetOpacity(), 1.0f);
  EXPECT_EQ(layer->transform(), initial_transform);
  EXPECT_EQ(layer->GetTargetTransform(), no_transform);
}

TEST_F(AppListPresenterWithScaleAnimationOnTabletModeTransitionTest,
       AbortedHideAnimationDoesNotChangeVisibility) {
  // Configure test observer.
  auto visibility_observer = std::make_unique<TestAppListControllerObserver>();
  auto* const app_list_controller = Shell::Get()->app_list_controller();
  app_list_controller->AddObserver(visibility_observer.get());

  // Switch to tablet mode and set normal animation duration.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  EXPECT_EQ(visibility_observer->visibility_changed_to_hidden_times(), 0);
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(visibility_observer->visibility_changed_to_hidden_times(), 0);
}

}  // namespace ash
