// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/apps_grid_view_folder_delegate.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/layer_animation_stopped_waiter.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace test {

namespace {

constexpr int kNumOfSuggestedApps = 3;

constexpr size_t kMaxItemsPerFolderPage =
    AppListFolderView::kMaxFolderColumns *
    AppListFolderView::kMaxPagedFolderRows;
constexpr size_t kMaxItemsInFolder = 48;

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;

  bool CreateShelfItemForAppId(
      const std::string& app_id,
      ShelfItem* item,
      std::unique_ptr<ShelfItemDelegate>* delegate) override {
    *item = ShelfItem();
    item->id = ShelfID(app_id);
    *delegate = std::make_unique<TestShelfItemDelegate>(item->id);
    return true;
  }
};

class PageFlipWaiter : public PaginationModelObserver {
 public:
  explicit PageFlipWaiter(PaginationModel* model) : model_(model) {
    model_->AddObserver(this);
  }

  PageFlipWaiter(const PageFlipWaiter&) = delete;
  PageFlipWaiter& operator=(const PageFlipWaiter&) = delete;

  ~PageFlipWaiter() override { model_->RemoveObserver(this); }

  void Wait() {
    DCHECK(!wait_);
    wait_ = true;

    ui_run_loop_ = std::make_unique<base::RunLoop>();
    ui_run_loop_->Run();
    wait_ = false;
  }

  void Reset() { selected_pages_.clear(); }

  const std::string& selected_pages() const { return selected_pages_; }

 private:
  // PaginationModelObserver overrides:
  void SelectedPageChanged(int old_selected, int new_selected) override {
    if (!selected_pages_.empty())
      selected_pages_ += ',';
    selected_pages_ += base::NumberToString(new_selected);

    if (wait_)
      ui_run_loop_->QuitWhenIdle();
  }

  std::unique_ptr<base::RunLoop> ui_run_loop_;
  PaginationModel* model_ = nullptr;
  bool wait_ = false;
  std::string selected_pages_;
};

// WindowDeletionWaiter waits for the specified window to be deleted.
class WindowDeletionWaiter : aura::WindowObserver {
 public:
  explicit WindowDeletionWaiter(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }

  WindowDeletionWaiter(const WindowDeletionWaiter&) = delete;
  WindowDeletionWaiter& operator=(const WindowDeletionWaiter&) = delete;

  ~WindowDeletionWaiter() override = default;

  void Wait() { run_loop_.Run(); }

 private:
  // WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    run_loop_.QuitWhenIdle();
  }

  base::RunLoop run_loop_;
  aura::Window* window_;
};

// Find the window with type WINDOW_TYPE_MENU and returns the firstly found one.
// Returns nullptr if no such window exists.
aura::Window* FindMenuWindow(aura::Window* root) {
  if (root->GetType() == aura::client::WINDOW_TYPE_MENU)
    return root;
  for (auto* child : root->children()) {
    auto* menu_in_child = FindMenuWindow(child);
    if (menu_in_child)
      return menu_in_child;
  }
  return nullptr;
}

// Dragging task to be run after page flip is observed.
class PostPageFlipTask : public PaginationModelObserver {
 public:
  PostPageFlipTask(PaginationModel* model, base::OnceClosure task)
      : model_(model), task_(std::move(task)) {
    model_->AddObserver(this);
  }

  PostPageFlipTask(const PostPageFlipTask&) = delete;
  PostPageFlipTask& operator=(const PostPageFlipTask&) = delete;

  ~PostPageFlipTask() override { model_->RemoveObserver(this); }

 private:
  // PaginationModelObserver overrides:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override {
  }
  void SelectedPageChanged(int old_selected, int new_selected) override {
    if (task_)
      std::move(task_).Run();
  }
  void TransitionStarted() override {}
  void TransitionChanged() override {}
  void TransitionEnded() override {}

  PaginationModel* model_;
  base::OnceClosure task_;
};

class TestSuggestedSearchResult : public TestSearchResult {
 public:
  TestSuggestedSearchResult() {
    set_display_type(SearchResultDisplayType::kChip);
    set_is_recommendation(true);
  }

  TestSuggestedSearchResult(const TestSuggestedSearchResult&) = delete;
  TestSuggestedSearchResult& operator=(const TestSuggestedSearchResult&) =
      delete;

  ~TestSuggestedSearchResult() override = default;
};

// Counts when the observed view's bounds change.
class BoundsChangeCounter : public views::ViewObserver {
 public:
  explicit BoundsChangeCounter(views::View* observed_view)
      : observed_view_(observed_view) {
    observed_view->AddObserver(this);
  }
  BoundsChangeCounter(const BoundsChangeCounter&) = delete;
  BoundsChangeCounter& operator=(const BoundsChangeCounter&) = delete;
  ~BoundsChangeCounter() override { observed_view_->RemoveObserver(this); }

  //  views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    ++bounds_change_count_;
  }

  int bounds_change_count() const { return bounds_change_count_; }

 private:
  views::View* const observed_view_;
  int bounds_change_count_ = 0;
};

}  // namespace

// Subclasses should set `is_rtl_`, `create_as_tablet_mode_`, etc. in their
// constructors to indicate which mode to test.
class AppsGridViewTest : public AshTestBase {
 public:
  AppsGridViewTest() = default;
  AppsGridViewTest(const AppsGridViewTest&) = delete;
  AppsGridViewTest& operator=(const AppsGridViewTest&) = delete;
  ~AppsGridViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (is_productivity_launcher_enabled_) {
      enabled_features.push_back(features::kProductivityLauncher);
    } else {
      disabled_features.push_back(features::kProductivityLauncher);
    }
    if (is_app_sort_enabled_) {
      enabled_features.push_back(features::kLauncherAppSort);
    } else {
      disabled_features.push_back(features::kLauncherAppSort);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    haptics_tracker_ = std::make_unique<HapticsTrackingTestInputController>();

    // Populate some suggested apps.
    search_model_ = std::make_unique<SearchModel>();
    for (size_t i = 0; i < kNumOfSuggestedApps; ++i) {
      search_model_->results()->Add(
          std::make_unique<TestSuggestedSearchResult>());
    }

    // Replace the model before the app list views are created, because some
    // views cache pointers to the model.
    model_ = std::make_unique<test::AppListTestModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, model_.get(), search_model_.get());

    // Show the app list.
    auto* helper = GetAppListTestHelper();
    if (create_as_tablet_mode_) {
      // The app list will be shown automatically when tablet mode is enabled.
      Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    } else if (features::IsProductivityLauncherEnabled()) {
      helper->ShowAppList();
    } else {
      // Show fullscreen so folders are available.
      helper->Show(GetPrimaryDisplay().id());
      helper->GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);
    }
    // Wait for any show animations to complete.
    base::RunLoop().RunUntilIdle();

    // Cache view pointers to make tests more concise.
    if (!create_as_tablet_mode_ && features::IsProductivityLauncherEnabled()) {
      // AppsGridView is scrollable in clamshell mode with AppListBubble.
      apps_grid_view_ = helper->GetScrollableAppsGridView();
      app_list_folder_view_ = helper->GetBubbleFolderView();
      search_box_view_ = helper->GetBubbleSearchBoxView();
    } else {
      app_list_view_ = helper->GetAppListView();
      app_list_folder_view_ = helper->GetFullscreenFolderView();
      auto* contents_view =
          app_list_view_->app_list_main_view()->contents_view();
      search_box_view_ = contents_view->GetSearchBoxView();
      // AppsGridView is paged in tablet mode and without AppListBubble.
      paged_apps_grid_view_ =
          contents_view->apps_container_view()->apps_grid_view();
      apps_grid_view_ = paged_apps_grid_view_;
      suggestions_container_ = contents_view->apps_container_view()
                                   ->suggestion_chip_container_view_for_test();
      expand_arrow_view_ = contents_view->expand_arrow_view();

      // In production, page flip duration > page transition > overscroll.
      SetPageFlipDurationForTest(paged_apps_grid_view_);
      page_flip_waiter_ =
          std::make_unique<PageFlipWaiter>(GetPaginationModel());
    }

    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view_);
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }

  void TearDown() override {
    page_flip_waiter_.reset();
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    haptics_tracker_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void AnimateFolderViewPageFlip(int target_page) {
    // Folders are only paged without productivity launcher enabled.
    DCHECK(!features::IsProductivityLauncherEnabled());
    PagedAppsGridView* paged_folder_apps_grid_view =
        static_cast<PagedAppsGridView*>(folder_apps_grid_view());
    DCHECK(paged_folder_apps_grid_view->pagination_model()->total_pages() >
           target_page);
    AppsGridViewTestApi folder_grid_test_api(paged_folder_apps_grid_view);
    SetPageFlipDurationForTest(paged_folder_apps_grid_view);
    PageFlipWaiter page_flip_waiter(
        paged_folder_apps_grid_view->pagination_model());
    paged_folder_apps_grid_view->pagination_model()->SelectPage(
        target_page, true /*animate*/);
    while (HasPendingPageFlip(paged_folder_apps_grid_view)) {
      page_flip_waiter.Wait();
    }
    folder_grid_test_api.LayoutToIdealBounds();
  }

  void SetPageFlipDurationForTest(PagedAppsGridView* apps_grid_view) {
    apps_grid_view->set_page_flip_delay_for_testing(base::Milliseconds(3));
    apps_grid_view->pagination_model()->SetTransitionDurations(
        base::Milliseconds(2), base::Milliseconds(1));
  }

  bool HasPendingPageFlip(PagedAppsGridView* apps_grid_view) {
    return apps_grid_view->page_flip_timer_.IsRunning() ||
           apps_grid_view->pagination_model()->has_transition();
  }

  const AppListConfig* GetAppListConfig() const {
    return apps_grid_view_->app_list_config();
  }

  AppListItemView* GetItemViewInAppsGridAt(int index,
                                           AppsGridView* grid_view) const {
    return grid_view->view_model()->view_at(index);
  }

  AppListItemView* GetItemViewInTopLevelGrid(int index) const {
    return GetItemViewInAppsGridAt(index, apps_grid_view_);
  }

  AppListItemView* GetItemViewInAppsGridForPoint(
      const gfx::Point& point,
      AppsGridView* grid_view) const {
    AppsGridViewTestApi temp_test_api(grid_view);
    const int selected_page = GetSelectedPage(grid_view);
    for (int i = 0; i < temp_test_api.AppsOnPage(selected_page); ++i) {
      GridIndex index(selected_page, i);
      AppListItemView* view = grid_view->GetViewAtIndex(index);
      gfx::Point view_origin = view->origin();
      views::View::ConvertPointToTarget(view->parent(), grid_view,
                                        &view_origin);
      if (gfx::Rect(view_origin, view->size()).Contains(point))
        return view;
    }
    return nullptr;
  }

  AppListItemView* GetItemViewForPoint(const gfx::Point& point) const {
    return GetItemViewInAppsGridForPoint(point, apps_grid_view_);
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(model_->top_level_item_list()->item_count(), 0u);
    return test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  int GetTilesPerPage(int page) const { return test_api_->TilesPerPage(page); }

  PaginationModel* GetPaginationModel() const {
    DCHECK(paged_apps_grid_view_) << "Only available in tablet mode or when "
                                     "ProductivityLauncher is disabled.";
    return paged_apps_grid_view_->pagination_model();
  }

  int GetSelectedPage(AppsGridView* grid_view) const {
    return grid_view->GetSelectedPage();
  }

  int GetTotalPages(AppsGridView* grid_view) const {
    return grid_view->GetTotalPages();
  }

  AppListFolderView* app_list_folder_view() const {
    return app_list_folder_view_;
  }

  AppsGridView* folder_apps_grid_view() const {
    return app_list_folder_view_->items_grid_view();
  }

  void SimulateKeyPress(ui::KeyboardCode key_code) {
    SimulateKeyPress(key_code, ui::EF_NONE);
  }

  void SimulateKeyPress(ui::KeyboardCode key_code, int flags) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code, flags);
    apps_grid_view_->OnKeyPressed(key_event);
  }

  void SimulateKeyReleased(ui::KeyboardCode key_code, int flags) {
    ui::KeyEvent key_event(ui::ET_KEY_RELEASED, key_code, flags);
    apps_grid_view_->OnKeyReleased(key_event);
  }

  // Points are in |apps_grid_view_|'s coordinates, and fixed for RTL.
  ui::GestureEvent SimulateTap(const gfx::Point& location) {
    ui::GestureEvent gesture_event(location.x(), location.y(), 0,
                                   base::TimeTicks(),
                                   ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    apps_grid_view_->OnGestureEvent(&gesture_event);
    return gesture_event;
  }

  // Simulates a tap on the point `location` if the test is in tablet mode.
  // Simulates a left click on the point otherwise.
  void SimulateLeftClickOrTapAt(const gfx::Point& location) {
    auto* event_generator = GetEventGenerator();
    if (create_as_tablet_mode_) {
      event_generator->GestureTapAt(location);
      return;
    }

    event_generator->MoveMouseTo(location);
    event_generator->ClickLeftButton();
  }

  // Simulates a long press on the point `location` if the test is in tablet
  // mode. Simulates a right click on the point otherwise. This function can be
  // used to open the context menu.
  void SimulateRightClickOrLongPressAt(const gfx::Point& location) {
    auto* event_generator = GetEventGenerator();
    if (create_as_tablet_mode_) {
      ui::GestureEvent gesture_event(
          location.x(), location.y(), 0, base::TimeTicks(),
          ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
      event_generator->Dispatch(&gesture_event);
      return;
    }

    event_generator->MoveMouseTo(location);
    event_generator->ClickRightButton();
  }

  // Tests that the order of item views in the AppsGridView is in accordance
  // with the order in the view model.
  void TestAppListItemViewIndice() {
    const views::ViewModelT<AppListItemView>* view_model =
        apps_grid_view_->view_model();
    DCHECK_GT(view_model->view_size(), 0);
    views::View* items_container = apps_grid_view_->items_container_;
    auto app_iter = items_container->FindChild(view_model->view_at(0));
    DCHECK(app_iter != items_container->children().cend());
    for (int i = 1; i < view_model->view_size(); ++i) {
      ++app_iter;
      ASSERT_NE(items_container->children().cend(), app_iter);
      EXPECT_EQ(view_model->view_at(i), *app_iter);
    }
  }

  views::View* GetItemsContainer() {
    return apps_grid_view_->items_container();
  }

  views::ViewModelT<PulsingBlockView>& GetPulsingBlocksModel() {
    return apps_grid_view_->pulsing_blocks_model();
  }

  views::View* GetCurrentGhostImageView() {
    return apps_grid_view_->current_ghost_view_;
  }

  // Calls the private method.
  static void DeleteItemAt(AppListItemList* item_list, size_t index) {
    item_list->DeleteItemAt(index);
  }

  // Calls the private method.
  void MoveItemInModel(AppListItemView* item_view, const GridIndex& target) {
    apps_grid_view_->MoveItemInModel(item_view->item(), target);
  }

  // Updates the layout of the container for the apps grid. Useful when the
  // test has added apps to the data model and is about to do an operation that
  // depends on item positions.
  void UpdateLayout() {
    if (!create_as_tablet_mode_ && features::IsProductivityLauncherEnabled())
      GetAppListTestHelper()->GetBubbleView()->Layout();
    else
      app_list_view_->Layout();
  }

  AppListItemView* InitiateDragForItemAtCurrentPageAt(
      AppsGridView::Pointer pointer,
      int row,
      int column,
      AppsGridView* apps_grid_view) {
    AppsGridViewTestApi test_api(apps_grid_view);
    const int selected_page = GetSelectedPage(apps_grid_view);
    GridIndex index(selected_page, row * apps_grid_view->cols() + column);
    AppListItemView* view = test_api.GetViewAtIndex(index);
    DCHECK(view);

    gfx::Point from = view->GetLocalBounds().CenterPoint();

    gfx::Point root_from = from;
    gfx::NativeWindow window = apps_grid_view->GetWidget()->GetNativeWindow();
    views::View::ConvertPointToWidget(view, &root_from);
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_from);

    view->InitiateDrag(from, root_from);
    current_drag_location_ = root_from;

    // Call UpdateDrag to trigger |apps_grid_view| change to cardified_state -
    // the cardified state starts only once the drag distance exceeds a drag
    // threshold, so the pointer has to sufficiently move from the original
    // position.
    gfx::Point from_in_grid = from;
    views::View::ConvertPointToTarget(view, apps_grid_view, &from_in_grid);
    UpdateDrag(pointer, from_in_grid + gfx::Vector2d(10, 10), apps_grid_view);
    return view;
  }

  // Updates the drag from the current drag location to the destination point
  // |to|. These coordinates are relative the |apps_grid_view| which may belong
  // to either the app list or an open folder view.
  void UpdateDrag(AppsGridView::Pointer pointer,
                  const gfx::Point& to,
                  AppsGridView* apps_grid_view,
                  int steps = 1) {
    // Check that the drag has been initialized.
    DCHECK(current_drag_location_);

    gfx::Point root_to(to);
    gfx::NativeWindow window = apps_grid_view->GetWidget()->GetNativeWindow();
    views::View::ConvertPointToWidget(apps_grid_view, &root_to);
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_to);

    for (int step = 1; step <= steps; step += 1) {
      gfx::Point drag_increment_point(*current_drag_location_);
      drag_increment_point += gfx::Vector2d(
          (root_to.x() - current_drag_location_->x()) * step / steps,
          (root_to.y() - current_drag_location_->y()) * step / steps);
      ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, to, drag_increment_point,
                                ui::EventTimeForNow(), 0, 0);
      apps_grid_view->UpdateDragFromItem(
          /*is_touch=*/pointer == AppsGridView::TOUCH, drag_event);
    }

    current_drag_location_ = root_to;
  }

  void EndDrag(AppsGridView* grid_view, bool cancel) {
    grid_view->EndDrag(cancel);
    current_drag_location_ = absl::nullopt;
  }

  // Simulate drag from the |from| point to either next or previous page's |to|
  // point.
  // Update drag to either next or previous page's |to| point.
  void UpdateDragToNeighborPage(bool next_page, const gfx::Point& to) {
    ASSERT_TRUE(paged_apps_grid_view_)
        << "Only available in tablet mode or when ProductivityLauncher is "
           "disabled.";
    const int selected_page = GetPaginationModel()->selected_page();
    DCHECK(selected_page >= 0 &&
           selected_page <= GetPaginationModel()->total_pages());

    // Calculate the point required to flip the page if an item is dragged to
    // it.
    const gfx::Rect apps_grid_bounds = paged_apps_grid_view_->GetLocalBounds();
    gfx::Point point_in_page_flip_buffer =
        gfx::Point(apps_grid_bounds.width() / 2,
                   next_page ? apps_grid_bounds.bottom() - 1 : 0);

    // Build the drag event which will be triggered after page flip.
    gfx::Point root_to(to);
    views::View::ConvertPointToWidget(paged_apps_grid_view_, &root_to);
    gfx::NativeWindow window = app_list_view_->GetWidget()->GetNativeWindow();
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_to);

    // Update dragging and relayout apps grid view after drag ends.
    PostPageFlipTask task(
        GetPaginationModel(), base::BindLambdaForTesting([&]() {
          ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, to, root_to,
                                    ui::EventTimeForNow(), 0, 0);
          paged_apps_grid_view_->UpdateDragFromItem(/*is_touch=*/false,
                                                    drag_event);
        }));
    page_flip_waiter_->Reset();
    UpdateDrag(AppsGridView::MOUSE, point_in_page_flip_buffer,
               paged_apps_grid_view_,
               /*steps=*/10);
    while (HasPendingPageFlip(paged_apps_grid_view_)) {
      page_flip_waiter_->Wait();
    }
    EndDrag(paged_apps_grid_view_, false /*cancel*/);
    test_api_->LayoutToIdealBounds();
  }

  gfx::Point GetDragIconCenter() {
    return test_api_->GetDragIconBoundsInAppsGridView().CenterPoint();
  }

  std::string GetItemMoveTypeHistogramName() {
    return paged_apps_grid_view_ ? "Apps.AppListAppMovingType"
                                 : "Apps.AppListBubbleAppMovingType";
  }

  int GetHapticTickEventsCount() const {
    return haptics_tracker_->GetSentHapticCount(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }

  // May be a PagedAppsGridView or a ScrollableAppsGridView depending on the
  // ProductivityLauncher flag and tablet mode.
  AppsGridView* apps_grid_view_ = nullptr;

  // May be owned by different parent views depending on the
  // ProductivityLauncher flag and tablet mode.
  AppListFolderView* app_list_folder_view_ = nullptr;
  SearchBoxView* search_box_view_ = nullptr;

  // These views exist in tablet mode and when ProductivityLauncher is disabled.
  PagedAppsGridView* paged_apps_grid_view_ = nullptr;
  AppListView* app_list_view_ = nullptr;  // Owned by native widget.
  SearchResultContainerView* suggestions_container_ =
      nullptr;                                    // Owned by |apps_grid_view_|.
  ExpandArrowView* expand_arrow_view_ = nullptr;  // Owned by |apps_grid_view_|.

  std::unique_ptr<AppListTestModel> model_;
  std::unique_ptr<SearchModel> search_model_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;

  // True if the test screen is configured to work with RTL locale.
  bool is_rtl_ = false;
  // True if feature ProductivityLauncher should be enabled.
  bool is_productivity_launcher_enabled_ = false;
  // True if feature LauncherAppSort should be enabled.
  bool is_app_sort_enabled_ = false;
  // True if we set the test on tablet mode.
  bool create_as_tablet_mode_ = false;

  std::unique_ptr<PageFlipWaiter> page_flip_waiter_;

 private:
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  base::test::ScopedFeatureList feature_list_;

  absl::optional<gfx::Point> current_drag_location_;

  // Used to track haptics events sent during drag.
  std::unique_ptr<HapticsTrackingTestInputController> haptics_tracker_;
};

// Tests that only run with ProductivityLauncher disabled, which disables the
// bubble launcher. These can be deleted when ProductivityLauncher is the
// default.
class AppsGridViewNonBubbleTest : public AppsGridViewTest {
 public:
  AppsGridViewNonBubbleTest() { is_productivity_launcher_enabled_ = false; }
};

// Test suite for clamshell mode, parameterized by feature ProductivityLauncher.
class AppsGridViewClamshellTest : public AppsGridViewTest,
                                  public testing::WithParamInterface<bool> {
 public:
  AppsGridViewClamshellTest() {
    is_productivity_launcher_enabled_ = GetParam();
  }
};
INSTANTIATE_TEST_SUITE_P(All, AppsGridViewClamshellTest, testing::Bool());

// Tests suite to test both tablet and clamshell mode behavior, additionally
// parameterized by feature ProductivityLauncher.
class AppsGridViewClamshellAndTabletTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppsGridViewClamshellAndTabletTest() {
    create_as_tablet_mode_ = std::get<0>(GetParam());
    is_productivity_launcher_enabled_ = std::get<1>(GetParam());
  }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewClamshellAndTabletTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests suite parameterized by RTL locale.
class AppsGridViewRTLTest : public AppsGridViewTest,
                            public testing::WithParamInterface<bool> {
 public:
  AppsGridViewRTLTest() { is_rtl_ = GetParam(); }
};
INSTANTIATE_TEST_SUITE_P(All, AppsGridViewRTLTest, testing::Bool());

// Tests suite for app list items drag and drop tests. These tests are
// parameterized by RTL locale and feature ProductivityLauncher.
class AppsGridViewDragTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppsGridViewDragTest() {
    is_rtl_ = std::get<0>(GetParam());
    is_productivity_launcher_enabled_ = std::get<1>(GetParam());
  }

  // AppsGridViewTest:
  void SetUp() override {
    AppsGridViewTest::SetUp();
    ShelfModel::Get()->SetShelfItemFactory(&shelf_item_factory_);
  }

  void TearDown() override {
    ShelfModel::Get()->SetShelfItemFactory(nullptr);
    AppsGridViewTest::TearDown();
  }

 private:
  // Shelf item factory required for test that drag from apps grid to shelf.
  ShelfItemFactoryFake shelf_item_factory_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewDragTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Drag and drop tests that are not expected to work with AppListBubble.
// Parameterized by RTL locale.
class AppsGridViewDragNonBubbleTest : public AppsGridViewTest,
                                      public testing::WithParamInterface<bool> {
 public:
  AppsGridViewDragNonBubbleTest() {
    is_rtl_ = GetParam();
    is_productivity_launcher_enabled_ = false;
  }
};
INSTANTIATE_TEST_SUITE_P(All, AppsGridViewDragNonBubbleTest, testing::Bool());

// Tests suite to verify behaviour exclusively to cardified state, parameterized
// by RTL locale and AppListBubble.
class AppsGridViewCardifiedStateTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppsGridViewCardifiedStateTest() {
    is_rtl_ = std::get<0>(GetParam());
    is_productivity_launcher_enabled_ = std::get<1>(GetParam());
    // The productivity launcher in clamshell mode does not use pages / cards,
    // so use tablet mode instead.
    create_as_tablet_mode_ = is_productivity_launcher_enabled_;
  }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewCardifiedStateTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Test suite for verifying tablet mode apps grid behaviour, parameterized by
// RTL locale and ProductivityLauncher feature.
class AppsGridViewTabletTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppsGridViewTabletTest() {
    is_rtl_ = std::get<0>(GetParam());
    is_productivity_launcher_enabled_ = std::get<1>(GetParam());
    create_as_tablet_mode_ = true;
  }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewTabletTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Test suite that tests apps sort works on all apps grid, parameterized by
// RTL locale and clamshell/tablet mode.
class AppsGridViewAppSortTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppsGridViewAppSortTest() {
    is_rtl_ = std::get<0>(GetParam());
    is_productivity_launcher_enabled_ = true;
    is_app_sort_enabled_ = true;
    create_as_tablet_mode_ = std::get<1>(GetParam());
  }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewAppSortTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// This does not test the font name or weight because ash_unittests returns
// different font lists than chrome (e.g. "DejaVu Sans" instead of "Roboto").
TEST_P(AppsGridViewClamshellTest, AppListItemViewFont) {
  model_->PopulateApps(1);
  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  EXPECT_EQ(12, item_view->title()->font_list().GetFontSize());
}

// This does not test the font name or weight because ash_unittests returns
// different font lists than chrome (e.g. "DejaVu Sans" instead of "Roboto").
TEST_P(AppsGridViewTabletTest, AppListItemViewFont) {
  model_->PopulateApps(1);
  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  if (is_productivity_launcher_enabled_)
    EXPECT_EQ(13, item_view->title()->font_list().GetFontSize());
  else
    EXPECT_EQ(12, item_view->title()->font_list().GetFontSize());
}

TEST_P(AppsGridViewClamshellTest, RemoveSelectedLastApp) {
  const int kTotalItems = 2;
  const int kLastItemIndex = kTotalItems - 1;

  model_->PopulateApps(kTotalItems);

  AppListItemView* last_view = GetItemViewInTopLevelGrid(kLastItemIndex);
  apps_grid_view_->SetSelectedView(last_view);
  model_->DeleteItem(model_->GetItemName(kLastItemIndex));

  EXPECT_FALSE(apps_grid_view_->IsSelectedView(last_view));

  // No crash happens.
  AppListItemView* view = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->SetSelectedView(view);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(view));
}

// Tests that UMA is properly collected when either a suggested or normal app is
// launched. Bubble launcher uses different metric names and does not use
// suggestion chips.
TEST_F(AppsGridViewNonBubbleTest, UMATestForLaunchingApps) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(5);
  UpdateLayout();

  // Select the first app in grid and launch it.
  LeftClickOn(GetItemViewInTopLevelGrid(0));

  // Test that histogram recorded app launch from grid.
  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps", 1 /* kAppListItem */,
      1 /* Times kAppListItem launched */);

  // Launch a suggested app.
  suggestions_container_->children().front()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));

  // Test that histogram recorded app launched from suggestion chip.
  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps", 2 /* kSuggestionChip */,
      1 /* Times kSuggestionChip Launched */);
}

// Tests that the item list changed without user operations; this happens on
// active user switch. See https://crbug.com/980082.
// TODO(jamescook): Investigate why this fails for bubble launcher.
TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseCrash) {
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  model_->PopulateApps(cols * 2);
  UpdateLayout();

  AppListItemView* view0 = GetItemViewInTopLevelGrid(0);
  model_->top_level_item_list()->MoveItem(0, cols + 2);

  // Make sure the logical location of the view.
  EXPECT_NE(view0, GetItemViewInTopLevelGrid(0));
  EXPECT_EQ(view0, GetItemViewInTopLevelGrid(cols + 2));

  // |view0| should be animating with layer.
  EXPECT_TRUE(view0->layer());
  EXPECT_TRUE(apps_grid_view_->IsAnimatingView(view0));

  test_api_->WaitForItemMoveAnimationDone();
  // |view0| layer should be cleared after the animation.
  EXPECT_FALSE(view0->layer());
  EXPECT_EQ(view0->bounds(), GetItemRectOnCurrentPageAt(1, 2));
}

// TODO(jamescook): Investigate why this fails for bubble launcher.
TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseAnimation) {
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  model_->PopulateApps(cols * 2);
  UpdateLayout();

  // NOTE: Dismissing the app list creates layers for item views as part of the
  // opacity animations, even in tests. https://crbug.com/1246567
  GetAppListTestHelper()->DismissAndRunLoop();
  ASSERT_FALSE(apps_grid_view_->GetWidget()->IsVisible());

  AppListItemView* view0 = GetItemViewInTopLevelGrid(0);
  model_->top_level_item_list()->MoveItem(0, cols + 2);

  // Make sure the logical location of the view.
  EXPECT_NE(view0, GetItemViewInTopLevelGrid(0));
  EXPECT_EQ(view0, GetItemViewInTopLevelGrid(cols + 2));

  // The item should be repositioned immediately when the widget is not visible.
  EXPECT_FALSE(apps_grid_view_->IsAnimatingView(view0));
  EXPECT_EQ(view0->bounds(), GetItemRectOnCurrentPageAt(1, 2));
}

// Tests that control + arrow while a suggested chip is focused does not crash.
// Productivity launcher does not use suggestion chips.
TEST_F(AppsGridViewNonBubbleTest, ControlArrowOnSuggestedChip) {
  model_->PopulateApps(5);
  UpdateLayout();
  suggestions_container_->children().front()->RequestFocus();

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(suggestions_container_->children().front(),
            apps_grid_view_->GetFocusManager()->GetFocusedView());
}

TEST_P(AppsGridViewClamshellTest, ItemTooltip) {
  std::string title("a");
  AppListItem* item = model_->CreateAndAddItem(title);
  model_->SetItemName(item, title);

  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_TRUE(
      title_label->GetTooltipText(title_label->bounds().CenterPoint()).empty());
  EXPECT_EQ(base::ASCIIToUTF16(title), title_label->GetText());
}

TEST_P(AppsGridViewRTLTest, ScrollSequenceHandledByAppListView) {
  base::HistogramTester histogram_tester;

  model_->PopulateApps(GetTilesPerPage(0) + 1);
  UpdateLayout();
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, 10));
  ui::GestureEvent scroll_end(apps_grid_view_origin.x(),
                              apps_grid_view_origin.y(), 0, base::TimeTicks(),
                              ui::GestureEventDetails(ui::ET_GESTURE_END));

  // Drag down on the app grid when on page 1, this should move the AppListView
  // and not move the AppsGridView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_FALSE(scroll_begin.handled());

  // Simulate redirecting the event to app list view through views hierarchy.
  app_list_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);

  // The following scroll update events will be sent to the view that handled
  // the scroll begin event.
  app_list_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_TRUE(app_list_view_->is_in_drag());
  ASSERT_EQ(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 0);

  app_list_view_->OnGestureEvent(&scroll_end);
  EXPECT_TRUE(scroll_end.handled());

  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 1);
}

TEST_P(AppsGridViewRTLTest, MouseScrollSequenceHandledByAppListView) {
  base::HistogramTester histogram_tester;

  model_->PopulateApps(GetTilesPerPage(0) + 1);
  UpdateLayout();
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  const gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  // Pick a point inside the `AppsGridView`, as well as arbitrary points below
  // and above it to drag to.
  gfx::Point drag_start_point = apps_grid_view_origin + gfx::Vector2d(0, 10);
  gfx::Point below_point = apps_grid_view_origin + gfx::Vector2d(0, 20);
  gfx::Point above_point = apps_grid_view_origin + gfx::Vector2d(0, -20);

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, apps_grid_view_origin,
                             apps_grid_view_origin, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  ui::MouseEvent drag_start(ui::ET_MOUSE_DRAGGED, drag_start_point,
                            drag_start_point, ui::EventTimeForNow(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  ui::MouseEvent drag_below(ui::ET_MOUSE_DRAGGED, below_point, below_point,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);

  ui::MouseEvent drag_above(ui::ET_MOUSE_DRAGGED, above_point, above_point,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);

  // Send a press event to set the `mouse_drag_start_point_` for scrolling
  apps_grid_view_->OnMouseEvent(&press_event);
  EXPECT_TRUE(press_event.handled());

  // Drag down on the app grid when on page 1, this should move the
  // `AppListView` and not move the `AppsGridView`. We have to send two drag
  // down events, `drag_start` and `drag_below`, to make sure `AppListView` has
  // its anchor point and starts dragging down. Event is manually passed to
  // `AppListview` in `AppsGridView::OnMouseEvent`
  apps_grid_view_->OnMouseEvent(&drag_start);
  EXPECT_TRUE(drag_start.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 0);

  apps_grid_view_->OnMouseEvent(&drag_below);
  EXPECT_TRUE(drag_below.handled());
  ASSERT_TRUE(app_list_view_->is_in_drag());
  ASSERT_EQ(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 0);

  // Now drag back up. Because we have dragged the launcher down, we want it to
  // continue dragging to allow the user to fully expand it. If we don't, the
  // launcher can end up in a "expanded" state with the launcher not reaching
  // the top of the screen and the app list scrolling.
  apps_grid_view_->OnMouseEvent(&drag_above);
  EXPECT_TRUE(drag_above.handled());
  ASSERT_TRUE(app_list_view_->is_in_drag());
  ASSERT_EQ(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 2);
  histogram_tester.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 0);
}

TEST_P(AppsGridViewRTLTest,
       OnGestureEventScrollSequenceHandleByPaginationController) {
  base::HistogramTester histogram_tester;

  model_->PopulateApps(GetTilesPerPage(0) + 1);
  UpdateLayout();
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, -1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, -10));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  // Drag up on the app grid when on page 1, this should move the AppsGridView
  // but not the AppListView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.ClamshellMode", 0);

  apps_grid_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_FALSE(app_list_view_->is_in_drag());
  ASSERT_NE(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.ClamshellMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "ClamshellMode",
      0);

  apps_grid_view_->OnGestureEvent(&scroll_end);

  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "ClamshellMode",
      1);
}

// Tests that taps between apps within the AppsGridView does not result in the
// AppList closing.
TEST_F(AppsGridViewTest, TapsBetweenAppsWontCloseAppList) {
  model_->PopulateApps(2);
  UpdateLayout();
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  gfx::Point empty_space = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();

  // Taps between apps should be handled to prevent them from going into
  // app_list
  ui::GestureEvent tap_between = SimulateTap(between_apps);
  EXPECT_TRUE(tap_between.handled());

  // Taps outside of occupied tiles should not be handled, that they may close
  // the app_list
  ui::GestureEvent tap_outside = SimulateTap(empty_space);
  EXPECT_FALSE(tap_outside.handled());
}

// The bubble launcher uses scrollable folders, so this test disables
// ProductivityLauncher.
TEST_F(AppsGridViewNonBubbleTest, PageResetAfterOpenFolder) {
  model_->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder. It should be at page 0.
  test_api_->PressItemAt(0);
  ASSERT_FALSE(features::IsProductivityLauncherEnabled());
  PaginationModel* pagination_model =
      static_cast<PagedAppsGridView*>(folder_apps_grid_view())
          ->pagination_model();
  EXPECT_EQ(3, pagination_model->total_pages());
  EXPECT_EQ(0, pagination_model->selected_page());

  // Select page 2.
  pagination_model->SelectPage(2, false /* animate */);
  EXPECT_EQ(2, pagination_model->selected_page());

  // Close the folder and reopen it. It should be at page 0.
  app_list_folder_view()->CloseFolderPage();
  test_api_->PressItemAt(0);
  EXPECT_EQ(3, pagination_model->total_pages());
  EXPECT_EQ(0, pagination_model->selected_page());
}

TEST_P(AppsGridViewClamshellTest, FolderColsAndRows) {
  // Populate folders with different number of apps.
  model_->CreateAndPopulateFolderWithApps(2);
  model_->CreateAndPopulateFolderWithApps(5);
  model_->CreateAndPopulateFolderWithApps(9);
  model_->CreateAndPopulateFolderWithApps(15);
  model_->CreateAndPopulateFolderWithApps(17);

  // Check the number of cols and rows for each opened folder.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  AppsGridViewTestApi folder_grid_test_api(items_grid_view);
  test_api_->PressItemAt(0);
  EXPECT_EQ(2, items_grid_view->view_model()->view_size());
  EXPECT_EQ(2, items_grid_view->cols());
  EXPECT_EQ(2, folder_grid_test_api.TilesPerPage(0));
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(1);
  EXPECT_EQ(5, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  EXPECT_EQ(6, folder_grid_test_api.TilesPerPage(0));
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(2);
  EXPECT_EQ(9, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  EXPECT_EQ(9, folder_grid_test_api.TilesPerPage(0));
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(3);
  EXPECT_EQ(15, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  EXPECT_EQ(16, folder_grid_test_api.TilesPerPage(0));
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(4);
  EXPECT_EQ(17, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  EXPECT_EQ(features::IsProductivityLauncherEnabled() ? 20 : 16,
            folder_grid_test_api.TilesPerPage(0));
  app_list_folder_view()->CloseFolderPage();
}

TEST_P(AppsGridViewClamshellTest, RemoveItemsInFolderShouldUpdateBounds) {
  // Populate two folders with different number of apps.
  model_->CreateAndPopulateFolderWithApps(2);
  AppListFolderItem* folder_2 = model_->CreateAndPopulateFolderWithApps(4);

  // Record the bounds of the folder view with 2 items in it.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  test_api_->PressItemAt(0);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect one_row_folder_view = items_grid_view->GetBoundsInScreen();
  app_list_folder_view()->CloseFolderPage();

  // Record the bounds of the folder view with 4 items in it and keep the folder
  // view open for further testing.
  test_api_->PressItemAt(1);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect two_rows_folder_view = items_grid_view->GetBoundsInScreen();
  EXPECT_NE(one_row_folder_view.size(), two_rows_folder_view.size());

  // Remove one item from the folder with 4 items. The bound should stay the
  // same as there are still two rows in the folder view.
  model_->DeleteItem(folder_2->item_list()->item_at(0)->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            two_rows_folder_view.size());

  // Remove another item from the folder. The bound should update and become the
  // folder view with one row.
  model_->DeleteItem(folder_2->item_list()->item_at(0)->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            one_row_folder_view.size());
}

TEST_P(AppsGridViewClamshellTest, AddItemsToFolderShouldUpdateBounds) {
  // Populate two folders with different number of apps.
  AppListFolderItem* folder_1 = model_->CreateAndPopulateFolderWithApps(2);
  model_->CreateAndPopulateFolderWithApps(4);

  // Record the bounds of the folder view with 4 items in it.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  test_api_->PressItemAt(1);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect two_rows_folder_view = items_grid_view->GetBoundsInScreen();
  app_list_folder_view()->CloseFolderPage();

  // Record the bounds of the folder view with 2 items in it and keep the folder
  // view open for further testing.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect one_row_folder_view = items_grid_view->GetBoundsInScreen();
  EXPECT_NE(one_row_folder_view.size(), two_rows_folder_view.size());

  // Add an item to the folder so that there are two rows in the folder view.
  model_->AddItemToFolder(model_->CreateItem("Extra 1"), folder_1->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            two_rows_folder_view.size());
  app_list_folder_view()->CloseFolderPage();

  // Create a folder with a full page of apps. Add an item to the folder should
  // not change the size of the folder view.
  AppListFolderItem* folder_full =
      model_->CreateAndPopulateFolderWithApps(kMaxItemsPerFolderPage);
  test_api_->PressItemAt(2);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect full_folder_view = items_grid_view->GetBoundsInScreen();

  model_->AddItemToFolder(model_->CreateItem("Extra 2"), folder_full->id());
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            full_folder_view.size());
  app_list_folder_view()->CloseFolderPage();
}

TEST_P(AppsGridViewRTLTest, ScrollDownShouldNotExitFolder) {
  const size_t kTotalItems = kMaxItemsPerFolderPage;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  gfx::Point apps_grid_view_origin =
      items_grid_view->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, 10));

  // Drag down on the items grid, this should be handled by items grid view and
  // the folder should not be closed.
  items_grid_view->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
}

// Tests that an app icon is selected when a menu is shown by click.
// TODO(jamescook): Investigate why this is broken with AppListBubble.
TEST_F(AppsGridViewTest, AppIconSelectedWhenMenuIsShown) {
  model_->PopulateApps(1);
  UpdateLayout();
  ASSERT_EQ(1u, model_->top_level_item_list()->item_count());
  AppListItemView* app = GetItemViewInTopLevelGrid(0);
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(app));

  // Send a mouse event which would show a context menu.
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(app->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(app));

  generator->ReleaseRightButton();
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(app));

  // Cancel the menu, |app| should no longer be selected.
  app->CancelContextMenu();
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(app));
}

// Tests that the context menu for app item appears at the right position.
TEST_P(AppsGridViewRTLTest, MenuAtRightPosition) {
  const size_t kItemsInPage = GetTilesPerPage(0);
  const size_t kPages = 2;
  model_->PopulateApps(kItemsInPage * kPages);
  UpdateLayout();

  auto* root = apps_grid_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  gfx::Rect root_bounds = root->GetBoundsInScreen();

  std::vector<int> pages_to_check = {1, 0};
  for (int i : pages_to_check) {
    GetPaginationModel()->SelectPage(i, /*animate=*/false);

    for (size_t j = 0; j < kItemsInPage; ++j) {
      const size_t idx = kItemsInPage * i + j;
      AppListItemView* item_view = GetItemViewInTopLevelGrid(idx);

      // Send a mouse event which would show a context menu.
      ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(),
                                 ui::EF_RIGHT_MOUSE_BUTTON,
                                 ui::EF_RIGHT_MOUSE_BUTTON);
      static_cast<views::View*>(item_view)->OnMouseEvent(&press_event);

      ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(),
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON);
      static_cast<views::View*>(item_view)->OnMouseEvent(&release_event);

      // Make sure that the menu is drawn on screen.
      auto* menu_window = FindMenuWindow(root);
      gfx::Rect menu_bounds = menu_window->GetBoundsInScreen();
      EXPECT_TRUE(root_bounds.Contains(menu_bounds))
          << "menu bounds for " << idx << "-th item " << menu_bounds.ToString()
          << " is outside of the screen bounds " << root_bounds.ToString();

      // CancelContextMenu doesn't remove the menu window immediately, so wait
      // for its actual deletion.
      WindowDeletionWaiter waiter(menu_window);
      item_view->CancelContextMenu();
      waiter.Wait();
    }
  }
}

TEST_P(AppsGridViewClamshellTest, ItemViewsDontHaveLayer) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  UpdateLayout();

  // Normally individual item-view does not have a layer.
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_FALSE(GetItemViewInTopLevelGrid(i)->layer());
}

TEST_P(AppsGridViewDragTest, DismissWhileDraggingDoesNotCrash) {
  model_->PopulateApps(2);
  UpdateLayout();
  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);

  // Non-zero animation durations are necessary to make sure we don't miss
  // crashes involving animation delegates. Specifically, `bounds_animator_` had
  // a use after free problem in the past.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetEventGenerator()->MoveMouseTo(
      item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseBy(20, 20);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ASSERT_TRUE(apps_grid_view_->drag_item());
  ASSERT_TRUE(apps_grid_view_->IsDragging());
  ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());

  GetAppListTestHelper()->Dismiss();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  // No crash
}

TEST_P(AppsGridViewDragTest, DismissWhileDraggingInFolderDoesNotCrash) {
  model_->CreateAndPopulateFolderWithApps(2);
  test_api_->Update();
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(1, folder_apps_grid_view());

  // Non-zero animation durations are necessary to make sure we don't miss
  // crashes involving animation delegates. Specifically, `bounds_animator_` had
  // a use after free problem in the past.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetEventGenerator()->MoveMouseTo(
      item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseBy(20, 20);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ASSERT_TRUE(folder_apps_grid_view()->drag_item());
  ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
  ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

  GetAppListTestHelper()->Dismiss();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  // No crash
}

TEST_P(AppsGridViewDragTest, ItemViewsHaveLayerDuringDrag) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_1 over item_0 creates a folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);

  // Each item view has its own layer during the drag.
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_TRUE(GetItemViewInTopLevelGrid(i)->layer());

  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, ItemViewsDontHaveLayerAfterDrag) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_1 over item_0 creates a folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->WaitForItemMoveAnimationDone();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // The layer should be destroyed after the dragging.
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_FALSE(GetItemViewInTopLevelGrid(i)->layer());
}

TEST_P(AppsGridViewDragTest, MouseDragItemIntoFolder) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_1 over item_0 creates a folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();

  EXPECT_EQ(kTotalItems - 1, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  AppListItem* item_0 = model_->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  AppListItem* item_1 = model_->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  std::string expected_items = folder_item->id() + ",Item 2";
  EXPECT_EQ(expected_items, model_->GetModelContent());

  EXPECT_EQ(is_productivity_launcher_enabled_,
            GetAppListTestHelper()->IsInFolderView());
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
    EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                    ->GetFolderNameViewForTest()
                    ->HasFocus());
  }
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, MouseDragSecondItemIntoFolder) {
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(1);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_2 to the folder adds Item_2 to the folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();

  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->GetModelContent());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  AppListItem* item_0 = model_->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  AppListItem* item_1 = model_->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  AppListItem* item_2 = model_->FindItem("Item 2");
  EXPECT_TRUE(item_2->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_2->folder_id());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToFolder) {
  model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(1);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_2 to the folder adds Item_2 to the folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  ASSERT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, DragIconHiddenImmediatelyWhenGridHides) {
  model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(1);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_2 to the folder adds Item_2 to the folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);

  // Start typing to close the apps page, and open search results.
  GetEventGenerator()->GestureTapAt(
      search_box_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);

  auto* helper = GetAppListTestHelper();
  if (is_productivity_launcher_enabled_) {
    // Wait for page switch animation.
    LayerAnimationStoppedWaiter().Wait(
        helper->GetBubbleAppsPage()->GetPageAnimationLayerForTest());
    ASSERT_FALSE(helper->GetBubbleAppsPage()->GetVisible());
    ASSERT_TRUE(helper->GetBubbleSearchPage()->GetVisible());
  } else {
    ASSERT_TRUE(helper->GetFullscreenSearchResultPageView()->GetVisible());
  }

  // Verify the drag icon is hidden immediately.
  EXPECT_FALSE(test_api_->GetDragIconLayer());
  EXPECT_FALSE(apps_grid_view_->drag_item());
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToCreateFolder) {
  model_->PopulateApps(3);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_1 over item_0 creates a folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  ASSERT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_EQ(is_productivity_launcher_enabled_,
            GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, FolderNotOpenedIfGridHidesDuringIconDrop) {
  model_->PopulateApps(3);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging item_1 over Item_0 creates a folder.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  ASSERT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  // Start typing to close the apps page, and open search results - verify the
  // folder does not get opened, and that the icon drop animation gets canceled.
  GetEventGenerator()->GestureTapAt(
      search_box_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);

  auto* helper = GetAppListTestHelper();
  if (is_productivity_launcher_enabled_) {
    // Wait for page switch animation.
    LayerAnimationStoppedWaiter().Wait(
        helper->GetBubbleAppsPage()->GetPageAnimationLayerForTest());
    ASSERT_FALSE(helper->GetBubbleAppsPage()->GetVisible());
    ASSERT_TRUE(helper->GetBubbleSearchPage()->GetVisible());
  } else {
    ASSERT_TRUE(helper->GetFullscreenSearchResultPageView()->GetVisible());
  }

  EXPECT_FALSE(test_api_->GetDragIconLayer());
  EXPECT_FALSE(helper->IsInFolderView());
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewClamshellTest, CheckFolderWithMultiplePagesContents) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsPerFolderPage;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);

  // Open the folder and check it's contents.
  test_api_->Update();
  test_api_->PressItemAt(0);

  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());
  EXPECT_EQ(4, folder_apps_grid_view()->cols());
  // Productivity launcher uses scrollable folders, not paged.
  if (!features::IsProductivityLauncherEnabled()) {
    EXPECT_EQ(16, AppsGridViewTestApi(folder_apps_grid_view()).TilesPerPage(0));
    EXPECT_EQ(1, GetTotalPages(folder_apps_grid_view()));
    EXPECT_EQ(0, GetSelectedPage(folder_apps_grid_view()));
  }
  EXPECT_TRUE(folder_apps_grid_view()->IsInFolder());
}

TEST_P(AppsGridViewDragTest, MouseDragItemOutOfFolder) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsPerFolderPage;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());
  // Drag the first folder child out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point. Note that we we are dropping
  // into the app list view not the folder view. The (0,1) spot is empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  AppListItem* item_0 = model_->FindItem("Item 0");
  AppListItem* item_1 = model_->FindItem("Item 1");
  EXPECT_FALSE(item_0->IsInFolder());
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  EXPECT_EQ(std::string(folder_item->id() + ",Item 0"),
            model_->GetModelContent());
  EXPECT_EQ(kTotalItems - 1, folder_item->ChildItemCount());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragOutOfFolder) {
  model_->CreateAndPopulateFolderWithApps(5);
  test_api_->Update();
  test_api_->PressItemAt(0);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point. Note that we we are dropping
  // into the app list view not the folder view. The (0,1) spot is empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  EXPECT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToAnotherFolder) {
  model_->CreateAndPopulateFolderWithApps(5);
  model_->CreateAndPopulateFolderWithApps(5);
  test_api_->Update();
  test_api_->PressItemAt(0);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  ASSERT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest,
       DragIconAnimatesAfterDragThatDeletesOriginalFolder) {
  model_->PopulateApps(2);
  model_->CreateSingleItemFolder("folder_id", "item_id");
  test_api_->Update();
  test_api_->PressItemAt(2);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the only folder child out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point. Note that we we are dropping
  // into the app list view not the folder view. The (0,3) spot is empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  EXPECT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterReorderDrag) {
  model_->PopulateApps(3);
  test_api_->Update();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first item to an empty slot in the grid.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, drop_point, apps_grid_view_, 5 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  ui::Layer* drag_icon_layer = test_api_->GetDragIconLayer();
  ASSERT_TRUE(drag_icon_layer);
  EXPECT_TRUE(drag_icon_layer->GetAnimator()->is_animating());
}

TEST_F(AppsGridViewNonBubbleTest, SwitchPageFolderItem) {
  // ProductivityLauncher does not use paged folders.
  ASSERT_FALSE(features::IsProductivityLauncherEnabled());

  // Creates a folder item with enough views to have a second page.
  const size_t kTotalItems = kMaxItemsPerFolderPage + 1;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);

  // Switch to second page and check it's contents.
  test_api_->Update();
  test_api_->PressItemAt(0);
  AnimateFolderViewPageFlip(1);

  EXPECT_EQ(1, GetSelectedPage(folder_apps_grid_view()));
  EXPECT_EQ(4, folder_apps_grid_view()->cols());
  EXPECT_EQ(16, AppsGridViewTestApi(folder_apps_grid_view()).TilesPerPage(0));
  EXPECT_TRUE(folder_apps_grid_view()->IsInFolder());
}

TEST_P(AppsGridViewDragNonBubbleTest, MouseDragItemOutOfFolderSecondPage) {
  // ProductivityLauncher does not use paged folders.
  ASSERT_FALSE(features::IsProductivityLauncherEnabled());

  // Creates a folder item with enough views to have a second page.
  const size_t kTotalItems = kMaxItemsPerFolderPage + 1;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  // Switch to second page.
  AnimateFolderViewPageFlip(1);
  // Drag the first folder child on the second page out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  // Calculate the target destination for our drag and update the drag to that
  // location.
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point. Note that we we are dropping
  // into the app list view not the folder view. The (0,1) spot is empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  // End the drag and assert that the item has been dragged out of the folder
  // and the app list's grid view has been updated accordingly.
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  AppListItem* item_0 = model_->FindItem("Item 0");
  AppListItem* item_16 = model_->FindItem("Item 16");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_FALSE(item_16->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  EXPECT_EQ(std::string(folder_item->id() + ",Item 16"),
            model_->GetModelContent());
  EXPECT_EQ(kTotalItems - 1, folder_item->ChildItemCount());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragNonBubbleTest, MouseDropItemFromFolderSecondPage) {
  // ProductivityLauncher does not use paged folders.
  ASSERT_FALSE(features::IsProductivityLauncherEnabled());

  // Creates a folder item with enough views to have a second page.
  const size_t kTotalItems = kMaxItemsPerFolderPage + 1;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  ASSERT_TRUE(folder_apps_grid_view()->IsInFolder());
  // Switch to second page.
  AnimateFolderViewPageFlip(1);

  // Fill the rest of the root grid view with new app list items. Leave 1 slot
  // open so dropping an item from a folder to the root level apps grid does not
  // cause a page overflow.
  model_->PopulateApps(GetTilesPerPage(0) - 2);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());

  // Drag the first folder child on the second page out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(1, GetHapticTickEventsCount());
  // Calculate the target destination for our drag and update the drag to that
  // location.
  gfx::Point empty_space =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
             10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Calculate the coordinates for the drop point. Note that we we are dropping
  // into the app list view not the folder view. We will drop between the folder
  // and the rest of the app list items in the root level apps grid view.
  gfx::Rect drop_tile_bounds = GetItemRectOnCurrentPageAt(0, 0);
  gfx::Point drop_point = is_rtl_ ? drop_tile_bounds.left_center()
                                  : drop_tile_bounds.right_center();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             5 /*steps*/);
  // End the drag and assert that the item has been dragged out of the folder
  // and the app list's grid view has been updated accordingly.
  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  AppListItem* item_0 = model_->FindItem("Item 0");
  AppListItem* item_16 = model_->FindItem("Item 16");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_FALSE(item_16->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  EXPECT_EQ(std::string(folder_item->id() +
                        ",Item 16,Item 17,Item 18,Item 19,Item 20,Item 21,Item "
                        "22,Item 23,Item 24,Item 25,Item 26,Item 27,Item "
                        "28,Item 29,Item 30,Item 31,Item 32,Item 33,Item 34"),
            model_->GetModelContent());
  EXPECT_EQ(kTotalItems - 1, folder_item->ChildItemCount());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolder) {
  // Create and add an almost full folder.
  const size_t kTotalItems = kMaxItemsInFolder - 1;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  ASSERT_FALSE(folder_item->IsFolderFull());
  // Create and add another item.
  model_->PopulateAppWithId(kTotalItems);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging one item into the folder, the folder should accept the item.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->LayoutToIdealBounds();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(kTotalItems + 1, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->IsFolderFull());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest, MouseDragExceedMaxItemsInFolder) {
  // Create and add a full folder.
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  EXPECT_TRUE(folder_item->IsFolderFull());

  // Create and add another 2 item.
  model_->PopulateAppWithId(kMaxItemsInFolder + 1);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Dragging the last item over the folder, the folder won't accept the new
  // item.
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->LayoutToIdealBounds();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxItemsInFolder, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->IsFolderFull());
}

TEST_P(AppsGridViewDragTest, MouseDragMovement) {
  // Create and add a full folder.
  model_->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  // Create and add another item.
  model_->PopulateAppWithId(kMaxItemsInFolder);
  UpdateLayout();
  AppListItemView* folder_view =
      GetItemViewForPoint(GetItemRectOnCurrentPageAt(0, 0).CenterPoint());
  // Drag the new item to the left so that the grid reorders.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).bottom_left();
  to.Offset(0, -1);  // Get a point inside the rect.
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  test_api_->LayoutToIdealBounds();

  // The grid now looks like | blank | folder |.
  EXPECT_EQ(nullptr, GetItemViewForPoint(
                         GetItemRectOnCurrentPageAt(0, 0).CenterPoint()));
  EXPECT_EQ(folder_view, GetItemViewForPoint(
                             GetItemRectOnCurrentPageAt(0, 1).CenterPoint()));

  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// Check that moving items around doesn't allow a drop to happen into a full
// folder.
TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolderWithMovement) {
  // Create and add a full folder.
  model_->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  // Create and add another item.
  model_->PopulateAppWithId(kMaxItemsInFolder);
  // Drag the new item to the left so that the grid reorders.
  AppListItemView* dragged_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 1, apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).bottom_left();
  to.Offset(0, -1);  // Get a point inside the rect.
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  gfx::Point folder_in_second_slot =
      GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point translated_destination = gfx::PointAtOffsetFromOrigin(
      folder_in_second_slot - dragged_view->origin());
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated_destination,
                            folder_in_second_slot, ui::EventTimeForNow(), 0, 0);
  apps_grid_view_->UpdateDragFromItem(/*is_touch=*/false, drag_event);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // The item should not have moved into the folder.
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxItemsInFolder, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

// Dragging an item towards its neighbours should not reorder until the drag is
// past the folder drop point.
TEST_P(AppsGridViewDragTest, MouseDragItemReorderBeforeFolderDropPoint) {
  model_->PopulateApps(2);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                 GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  gfx::Vector2d drag_vector(-half_tile_width - 4, 0);
  // Flip drag vector in rtl.
  if (is_rtl_)
    drag_vector.set_x(-drag_vector.x());

  // Drag left but stop before the folder dropping circle.
  UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
             10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 1"), model_->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderAfterFolderDropPoint) {
  model_->PopulateApps(2);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                 GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  gfx::Vector2d drag_vector(
      -2 * half_tile_width -
          GetAppListConfig()->folder_dropping_circle_radius() - 4,
      0);
  // Flip drag vector in rtl.
  if (is_rtl_)
    drag_vector.set_x(-drag_vector.x());

  // Drag left, past the folder dropping circle.
  UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
             10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 1,Item 0"), model_->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragDownOneRow) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  model_->PopulateApps(7);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                 GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                    GetItemRectOnCurrentPageAt(0, 0).y();
  gfx::Vector2d drag_vector(-half_tile_width, tile_height);
  // Flip drag vector in rtl.
  if (is_rtl_)
    drag_vector.set_x(-drag_vector.x());

  // Drag down, between apps 5 and 6. The gap should open up, making space for
  // app 1 in the bottom left.
  UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
             10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 1,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragUpOneRow) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  model_->PopulateApps(7);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 1, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(1, 0).CenterPoint();
  int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                 GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                    GetItemRectOnCurrentPageAt(0, 0).y();
  gfx::Vector2d drag_vector(half_tile_width, -tile_height);
  // Flip drag vector in rtl.
  if (is_rtl_)
    drag_vector.set_x(-drag_vector.x());

  // Drag up, between apps 0 and 2. The gap should open up, making space for app
  // 1 in the top right.
  UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
             10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->LayoutToIdealBounds();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 5,Item 1,Item 2,Item 3,Item 4,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragPastLastApp) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  model_->PopulateApps(7);
  UpdateLayout();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                 GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                    GetItemRectOnCurrentPageAt(0, 0).y();
  // Drag over by half a tile and down by one tile. This ends the drag to the
  // right of the last item.
  gfx::Vector2d drag_vector(half_tile_width, tile_height);
  // Flip drag vector in rtl.
  if (is_rtl_)
    drag_vector.set_x(-drag_vector.x());

  UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
             10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 6,Item 1"),
            model_->GetModelContent());
  TestAppListItemViewIndice();
}

// Dragging folder over item_2 should leads to re-ordering these two items.
TEST_P(AppsGridViewDragTest, MouseDragFolderOverItemReorder) {
  size_t kTotalItems = 2;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  model_->PopulateAppWithId(kTotalItems);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_);
  EndDrag(apps_grid_view_, false /*cancel*/);
  test_api_->LayoutToIdealBounds();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ("Item 2", model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(1)->id());
  TestAppListItemViewIndice();
}

// Canceling drag should keep existing order.
TEST_P(AppsGridViewDragTest, MouseDragWithCancelKeepsOrder) {
  size_t kTotalItems = 2;
  model_->PopulateApps(kTotalItems);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, true /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 1"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Deleting an item keeps remaining intact.
TEST_P(AppsGridViewDragTest, MouseDragWithDeleteItemKeepsOrder) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  model_->DeleteItem(model_->GetItemName(2));
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 1"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Adding a launcher item cancels the drag and respects the order.
TEST_P(AppsGridViewDragTest, MouseDragWithAddItemKeepsOrder) {
  size_t kTotalItems = 2;
  model_->PopulateApps(kTotalItems);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  model_->CreateAndAddItem("Extra");
  EndDrag(apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(std::string("Item 0,Item 1,Extra"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Regression test for crash bug. https://crbug.com/1166011.
TEST_P(AppsGridViewClamshellAndTabletTest, MoveItemInModelPastEndOfList) {
  model_->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // I speculate that the item list is missing an item, but PagedViewStructure
  // doesn't know about it. This could happen if an item was deleted during a
  // period that AppsGridView was not observing the list.
  AppListItemList* item_list = model_->top_level_item_list();
  item_list->RemoveObserver(apps_grid_view_);
  DeleteItemAt(item_list, 19);
  item_list->AddObserver(apps_grid_view_);
  ASSERT_EQ(19u, item_list->item_count());

  // Try to move the first item to the end of the page. PagedViewStructure is
  // out of sync with AppListItemList, so it will return a target item list
  // index off the end of the list.
  AppListItemView* first_item = GetItemViewInTopLevelGrid(0);
  MoveItemInModel(first_item, GridIndex(0, 19));

  // No crash. Item moved to the end.
  ASSERT_EQ(19u, item_list->item_count());
  EXPECT_EQ("Item 0", item_list->item_at(18)->id());
}

// Test that control+arrow swaps app within the same page.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowSwapsAppsWithinSamePage) {
  model_->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* moving_item = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Test that moving left from 0,0 does not move the app.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, test_api_->GetViewAtVisualIndex(0, 0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving up from 0,0 does not move the app.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewInTopLevelGrid(0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving right from 0,0 results in a swap with the item adjacent.
  AppListItemView* swapped_item = GetItemViewInTopLevelGrid(1);
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewInTopLevelGrid(1));
  EXPECT_EQ(swapped_item, GetItemViewInTopLevelGrid(0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving down from 0,1 results in a swap with the item at 1,1.
  swapped_item = GetItemViewInTopLevelGrid(apps_grid_view_->cols() + 1);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item,
            GetItemViewInTopLevelGrid(apps_grid_view_->cols() + 1));
  EXPECT_EQ(swapped_item, GetItemViewInTopLevelGrid(1));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving left from 1,1 results in a swap with the item at 1,0.
  swapped_item = GetItemViewInTopLevelGrid(apps_grid_view_->cols());
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewInTopLevelGrid(apps_grid_view_->cols()));
  EXPECT_EQ(swapped_item,
            GetItemViewInTopLevelGrid(apps_grid_view_->cols() + 1));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving up from 1,0 results in a swap with the item at 0,0.
  swapped_item = GetItemViewInTopLevelGrid(0);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewInTopLevelGrid(0));
  EXPECT_EQ(swapped_item, GetItemViewInTopLevelGrid(apps_grid_view_->cols()));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));
}

// Tests that histograms are recorded when apps are moved with control+arrow.
TEST_P(AppsGridViewClamshellAndTabletTest, ControlArrowRecordsHistogramBasic) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* moving_item = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 2);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 3);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 4);
}

// Test that histograms do not record when the keyboard move is a no-op.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMove) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* moving_item = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Make 2 no-op moves and one successful move from 0,0 and expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 1);
}

// Tests that an item is scrolled to visible position if moved to initially
// invisible grid slot, and that histograms for a long keyboard sequence move
// gets recorded only once.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDownwardMoveSequenceToSlotNotInitiallyVisible) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(40);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* moving_item = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  auto app_list_item_view_visible = [this](const views::View* view) -> bool {
    return apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
        view->GetBoundsInScreen());
  };

  // The test will repeatedly move an item down until the target slot is in a
  // position that was initially not visible (either on different apps grid
  // page, or clipped withing scrollable apps grid).
  // Find the target row:
  int first_hidden_row = 0;
  while (true) {
    const views::View* first_item_in_row =
        GetItemViewInTopLevelGrid(first_hidden_row * apps_grid_view_->cols());
    ASSERT_TRUE(first_item_in_row);
    if (!app_list_item_view_visible(first_item_in_row))
      break;
    ++first_hidden_row;
  }

  // Make that a series of moves when the control key is left pressed and expect
  // one histogram is recorded.
  for (int i = 0; i < first_hidden_row; ++i) {
    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  }
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  // Verify that the view is visible at the end of the move.
  EXPECT_TRUE(app_list_item_view_visible(moving_item));

  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 1);
}

// Tests that moving an app down when it is directly below a gap results in a
// swap with the closest item.
TEST_P(AppsGridViewClamshellAndTabletTest, ControlArrowDownToGapOnSamePage) {
  // Add two rows of apps, one full and one with just one app.
  model_->PopulateApps(apps_grid_view_->cols() + 1);

  // Select the far right item.
  AppListItemView* moving_item =
      GetItemViewInTopLevelGrid(apps_grid_view_->cols() - 1);
  AppListItemView* swapped_item =
      GetItemViewInTopLevelGrid(apps_grid_view_->cols());
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Press down to move the app to the next row. It should take the place of the
  // app on the next row that is closes to the column of |moving_item|.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewInTopLevelGrid(apps_grid_view_->cols()));
  EXPECT_EQ(swapped_item,
            GetItemViewInTopLevelGrid(apps_grid_view_->cols() - 1));
}

// Tests that moving an app up/down/left/right to a full page results in the app
// at the destination slot moving to the source slot (ie. a swap).
TEST_P(AppsGridViewTabletTest, ControlArrowSwapsBetweenFullPages) {
  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  apps_grid_view_->UpdatePagedViewStructure();

  // For every item in the first row, ensure an upward move results in the item
  // swapping places with the item directly above it.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    GetPaginationModel()->SelectPage(1, false /*animate*/);
    const GridIndex moved_view_index(1, i);
    apps_grid_view_->GetFocusManager()->SetFocusedView(
        test_api_->GetViewAtIndex(moved_view_index));

    const GridIndex swapped_view_index(
        0, GetTilesPerPage(0) - apps_grid_view_->cols() + i);
    AppListItemView* moved_view = test_api_->GetViewAtIndex(moved_view_index);
    AppListItemView* swapped_view =
        test_api_->GetViewAtIndex(swapped_view_index);

    SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

    // |swapped_view| and |moved_view| should swap places when moving up to a
    // full page.
    EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
    EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
  }

  // For every item in the last row of a full page, ensure a downward move
  // results in the item swapping places when the target position is occupied.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    GetPaginationModel()->SelectPage(0, false /*animate*/);
    const GridIndex moved_view_index(
        0, GetTilesPerPage(0) - apps_grid_view_->cols() + i);
    apps_grid_view_->GetFocusManager()->SetFocusedView(
        test_api_->GetViewAtIndex(moved_view_index));

    const GridIndex swapped_view_index(1, i);
    AppListItemView* moved_view = test_api_->GetViewAtIndex(moved_view_index);
    AppListItemView* swapped_view =
        test_api_->GetViewAtIndex(swapped_view_index);

    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

    // |swapped_view| and |moved_view| should swap places when moving up to a
    // full page.
    EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
    EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
    EXPECT_EQ(1, GetPaginationModel()->selected_page());
  }

  // For the final item on the first page, moving right to a full page should
  // swap with the first item on the next page.
  GetPaginationModel()->SelectPage(0, false /*animate*/);
  GridIndex moved_view_index(0, GetTilesPerPage(0) - 1);
  GridIndex swapped_view_index(1, 0);
  AppListItemView* moved_view = test_api_->GetViewAtIndex(moved_view_index);
  AppListItemView* swapped_view = test_api_->GetViewAtIndex(swapped_view_index);
  apps_grid_view_->GetFocusManager()->SetFocusedView(
      test_api_->GetViewAtIndex(moved_view_index));

  SimulateKeyPress(is_rtl_ ? ui::VKEY_LEFT : ui::VKEY_RIGHT,
                   ui::EF_CONTROL_DOWN);

  EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
  EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
  EXPECT_EQ(1, GetPaginationModel()->selected_page());

  // For the first item on the second page, moving left to a full page should
  // swap with the first item on the previous page.
  swapped_view_index = moved_view_index;
  moved_view_index = GridIndex(1, 0);
  moved_view = test_api_->GetViewAtIndex(moved_view_index);
  swapped_view = test_api_->GetViewAtIndex(swapped_view_index);

  SimulateKeyPress(is_rtl_ ? ui::VKEY_RIGHT : ui::VKEY_LEFT,
                   ui::EF_CONTROL_DOWN);

  EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
  EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
}

// Test that a page can be created while moving apps with the control+arrow.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDownAndRightCreatesNewPage) {
  base::HistogramTester histogram_tester;
  const int kFirstPageSize = paged_apps_grid_view_ ? GetTilesPerPage(0) : 20;
  model_->PopulateApps(kFirstPageSize);

  // Focus the last item on the page.
  AppListItemView* moving_item = GetItemViewInTopLevelGrid(kFirstPageSize - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Test that pressing control-right creates a new page.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  bool expect_page_creation = !is_productivity_launcher_enabled_;
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7,
                                     expect_page_creation ? 1 : 0);

  if (expect_page_creation) {
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(1, 0)));
  } else {
    EXPECT_EQ(moving_item,
              test_api_->GetViewAtIndex(GridIndex(0, kFirstPageSize - 1)));
  }
  EXPECT_EQ(kFirstPageSize - (expect_page_creation ? 1 : 0),
            test_api_->AppsOnPage(0));

  if (paged_apps_grid_view_) {
    EXPECT_EQ(expect_page_creation ? 1 : 0,
              GetPaginationModel()->selected_page());
    EXPECT_EQ(expect_page_creation ? 2 : 1,
              GetPaginationModel()->total_pages());
  }

  // Reset by moving the app back to the previous page.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7,
                                     expect_page_creation ? 2 : 0);

  // Test that control-down creates a new page.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  // The slot where |moving_item| originated should be empty because items get
  // dumped on pages with room, and only swap if the destination page is full.
  EXPECT_EQ(expect_page_creation ? nullptr : moving_item,
            test_api_->GetViewAtIndex(GridIndex(0, kFirstPageSize - 1)));
  if (expect_page_creation) {
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(1, 0)));
  } else {
    EXPECT_EQ(moving_item,
              test_api_->GetViewAtIndex(GridIndex(0, kFirstPageSize - 1)));
  }
  EXPECT_EQ(kFirstPageSize - (expect_page_creation ? 1 : 0),
            test_api_->AppsOnPage(0));
  EXPECT_EQ(expect_page_creation ? 1 : 0, test_api_->AppsOnPage(1));
  if (paged_apps_grid_view_) {
    EXPECT_EQ(expect_page_creation ? 1 : 0,
              GetPaginationModel()->selected_page());
    EXPECT_EQ(expect_page_creation ? 2 : 1,
              GetPaginationModel()->total_pages());
  }
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7,
                                     expect_page_creation ? 3 : 0);
}

// Tests that a page can be deleted if a lonely app is moved down or right to
// another page.
TEST_F(AppsGridViewTest, ControlArrowUpOrLeftRemovesPage) {
  base::HistogramTester histogram_tester;
  // Move an app so it is by itself on page 1.
  model_->PopulateApps(GetTilesPerPage(0));
  AppListItemView* moving_item =
      GetItemViewInTopLevelGrid(GetTilesPerPage(0) - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Move the app up, test that the page is deleted.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 2);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());

  // Move the app to be by itself again on page 1.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 3);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Move the app left, test that the page is deleted.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 4);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());
}

// Tests that moving a lonely app on the last page down is a no-op when there
// are no pages below.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDownOnLastAppOnLastPage) {
  base::HistogramTester histogram_tester;
  const int kItemCount = paged_apps_grid_view_ ? GetTilesPerPage(0) + 1 : 21;
  model_->PopulateApps(kItemCount);
  AppListItemView* moving_item = GetItemViewInTopLevelGrid(kItemCount - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  if (paged_apps_grid_view_) {
    EXPECT_EQ(1, GetPaginationModel()->selected_page());
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
  }

  // Move the app right, test that nothing changes and no histograms are
  // recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 0);
  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 0);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType", 6, 0);
  if (paged_apps_grid_view_) {
    EXPECT_EQ(1, GetPaginationModel()->selected_page());
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
  }

  // Move the app down, test that nothing changes and no histograms are
  // recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 0);
  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 0);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType", 6, 0);
  if (paged_apps_grid_view_) {
    EXPECT_EQ(1, GetPaginationModel()->selected_page());
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
  }
}

// Test that moving an item down or right when it is by itself on a page with a
// page below results in the page deletion.
TEST_F(AppsGridViewTest, ControlArrowDownOrRightRemovesPage) {
  // Move an app so it is by itself on page 1, with another app on page 2.
  model_->PopulateApps(GetTilesPerPage(0));
  AppListItemView* moving_item =
      GetItemViewInTopLevelGrid(GetTilesPerPage(0) - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  // The lonely app is selected on page 1, with a page below it containing one
  // app.
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Test that moving the app on page 1 down, deletes the second page and
  // creates a final page with 2 apps.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  EXPECT_EQ(2, test_api_->AppsOnPage(1));

  // Create a third page, with an app by itself.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Test that moving the app right moves the selected app to the third page,
  // and the second page is deleted.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  EXPECT_EQ(2, test_api_->AppsOnPage(1));
}

// Tests that control + shift + arrow puts |selected_item_| into a folder or
// creates a folder if one does not exist.
TEST_P(AppsGridViewClamshellAndTabletTest, ControlShiftArrowFoldersItemBasic) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(3 * apps_grid_view_->cols());
  UpdateLayout();
  // Select the first item in the grid, folder it with the item to the right.
  AppListItemView* first_item = GetItemViewInTopLevelGrid(0);
  const std::string first_item_id = first_item->item()->id();
  const std::string second_item_id = GetItemViewInTopLevelGrid(1)->item()->id();
  const gfx::Rect expected_folder_view_bounds = first_item->GetBoundsInScreen();

  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  // Press an arrow key to engage keyboard traversal in fullscreen launcher.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN);

  // Focus first item, and folder it.
  apps_grid_view_->GetFocusManager()->SetFocusedView(first_item);
  event_generator->PressAndReleaseKey(ui::VKEY_RIGHT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items.
  AppListItemView* new_folder = GetItemViewInTopLevelGrid(0);
  EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 1);

  // With productivity launcher enabled, the folder is expected to get opened
  // after creation.
  EXPECT_EQ(!is_productivity_launcher_enabled_,
            apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(is_productivity_launcher_enabled_,
            GetAppListTestHelper()->IsInFolderView());
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
    EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                    ->GetFolderNameViewForTest()
                    ->HasFocus());

    // Close the folder.
    event_generator->PressAndReleaseKey(ui::VKEY_UP);
    event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
    EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
    EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());
  }
  ASSERT_TRUE(new_folder->HasFocus());
  ASSERT_TRUE(apps_grid_view_->IsSelectedView(new_folder));

  // Test that, when a folder is selected, control+shift+arrow does nothing.
  event_generator->PressAndReleaseKey(ui::VKEY_RIGHT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 1);

  // Move selection to the item to the right of the folder and put it in the
  // folder.
  apps_grid_view_->GetFocusManager()->SetFocusedView(
      GetItemViewInTopLevelGrid(1));

  event_generator->PressAndReleaseKey(ui::VKEY_LEFT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 2);

  // Move selection to the item below the folder and put it in the folder.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN);
  event_generator->PressAndReleaseKey(ui::VKEY_UP,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 3);

  // Move the folder to the second row, then put the item above the folder in
  // the folder.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  event_generator->PressAndReleaseKey(ui::VKEY_UP);
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_EQ(5u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 4);
}

// Tests that control + shift + left arrow puts |selected_item_| creates a
// folder if one does not exist.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlShiftLeftArrowFoldersItemBasic) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(3 * apps_grid_view_->cols());
  UpdateLayout();
  // Select the first item in the grid, folder it with the item to the right.
  AppListItemView* first_item = GetItemViewInTopLevelGrid(0);
  const std::string first_item_id = first_item->item()->id();
  AppListItemView* second_item = GetItemViewInTopLevelGrid(1);
  const std::string second_item_id = second_item->item()->id();
  gfx::Rect expected_folder_view_bounds = first_item->GetBoundsInScreen();

  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  // Press an arrow key to engage keyboard traversal in fullscreen launcher.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN);

  // Focus second item, and folder it.
  apps_grid_view_->GetFocusManager()->SetFocusedView(second_item);
  event_generator->PressAndReleaseKey(ui::VKEY_LEFT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items.
  AppListItemView* new_folder = GetItemViewInTopLevelGrid(0);
  EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(),
                                     kMoveByKeyboardIntoFolder, 1);

  // With productivity launcher enabled, the folder is expected to get opened
  // after creation.
  EXPECT_EQ(!is_productivity_launcher_enabled_,
            apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(is_productivity_launcher_enabled_,
            GetAppListTestHelper()->IsInFolderView());
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
    EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                    ->GetFolderNameViewForTest()
                    ->HasFocus());

    // Close the folder.
    event_generator->PressAndReleaseKey(ui::VKEY_UP);
    event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
    EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
    EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());
  }
  ASSERT_TRUE(new_folder->HasFocus());
  ASSERT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
}

// Tests that foldering an item that is on a different page fails.
TEST_P(AppsGridViewTabletTest, ControlShiftArrowFailsToFolderAcrossPages) {
  model_->PopulateApps(GetTilesPerPage(0) + GetTilesPerPage(1));
  UpdateLayout();

  // For every item on the last row of the first page, test that foldering to
  // the next page fails.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    const GridIndex moved_view_index(
        0, GetTilesPerPage(0) - apps_grid_view_->cols() + i);
    AppListItemView* attempted_folder_view =
        test_api_->GetViewAtIndex(moved_view_index);
    apps_grid_view_->GetFocusManager()->SetFocusedView(attempted_folder_view);

    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

    EXPECT_EQ(attempted_folder_view,
              test_api_->GetViewAtIndex(moved_view_index));
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
  }
  // The last item on the col is selected, try moving right and test that that
  // fails as well.
  GridIndex moved_view_index(0, GetTilesPerPage(0) - 1);
  AppListItemView* attempted_folder_view =
      test_api_->GetViewAtIndex(moved_view_index);

  SimulateKeyPress(is_rtl_ ? ui::VKEY_LEFT : ui::VKEY_RIGHT,
                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_EQ(attempted_folder_view, test_api_->GetViewAtIndex(moved_view_index));

  // Move to the second page and test that foldering up to a new page fails.
  SimulateKeyPress(ui::VKEY_DOWN);

  // Select the first item on the second page.
  moved_view_index = GridIndex(1, 0);
  attempted_folder_view = test_api_->GetViewAtIndex(moved_view_index);

  // Try to folder left to the previous page, it  should fail.
  SimulateKeyPress(is_rtl_ ? ui::VKEY_RIGHT : ui::VKEY_LEFT,
                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_EQ(attempted_folder_view, test_api_->GetViewAtIndex(moved_view_index));

  // For every item on the first row of the second page, test that foldering to
  // the next page fails.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    const GridIndex moved_view_index(1, i);
    AppListItemView* attempted_folder_view =
        test_api_->GetViewAtIndex(moved_view_index);
    apps_grid_view_->GetFocusManager()->SetFocusedView(attempted_folder_view);

    SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

    EXPECT_EQ(attempted_folder_view,
              test_api_->GetViewAtIndex(moved_view_index));
    EXPECT_EQ(1, GetPaginationModel()->selected_page());
  }
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       KeyboardReparentFromFolderInLastVisibleSlot) {
  // Create grid with a folder on the last slot in a page (or for scrollable
  // grid, the last slot in the page with enough items that the slot is
  // initially not in the visible part of the grid).
  const int kTopLevelItemCount = paged_apps_grid_view_
                                     ? GetTilesPerPage(0) + GetTilesPerPage(1)
                                     : apps_grid_view_->cols() * 8;
  model_->PopulateApps(kTopLevelItemCount - 1);
  const AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  apps_grid_view_->UpdatePagedViewStructure();
  UpdateLayout();

  AppListItemView* folder_view = apps_grid_view_->view_model()->view_at(
      apps_grid_view_->view_model()->view_size() - 1);
  ASSERT_TRUE(folder_view->is_folder());
  EXPECT_FALSE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      folder_view->GetBoundsInScreen()));

  folder_view->RequestFocus();
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      folder_view->GetBoundsInScreen()));
  const gfx::Rect original_folder_view_bounds =
      folder_view->GetBoundsInScreen();

  // Open the folder.
  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(original_folder_view_bounds, folder_view->GetBoundsInScreen());

  const AppListItemView* reparented_item_view =
      folder_apps_grid_view()->view_model()->view_at(0);
  ASSERT_TRUE(reparented_item_view);
  ASSERT_TRUE(reparented_item_view->item());

  std::string reparented_item_id = reparented_item_view->item()->id();
  EXPECT_EQ(base::StringPrintf("Item %d", kTopLevelItemCount - 1),
            reparented_item_id);
  ASSERT_TRUE(reparented_item_view->HasFocus());

  // Reparent the item to the slot after the folder view, which should be the
  // last spot in the grid.
  const ui::KeyboardCode forward_key = is_rtl_ ? ui::VKEY_LEFT : ui::VKEY_RIGHT;
  event_generator->PressAndReleaseKey(forward_key,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_EQ(folder_item, model_->FindItem(folder_id));
  EXPECT_EQ(2u, folder_item->ChildItemCount());

  const AppListItemView* last_top_level_item_view =
      apps_grid_view_->view_model()->view_at(
          apps_grid_view_->view_model()->view_size() - 1);
  // Verify the view is within visible grid bounds, and that it has focus.
  EXPECT_EQ(reparented_item_id, last_top_level_item_view->item()->id());
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      last_top_level_item_view->GetBoundsInScreen()));
  EXPECT_TRUE(last_top_level_item_view->HasFocus());

  // In paged apps grid, the item should have been moved to a new page.
  if (paged_apps_grid_view_) {
    EXPECT_EQ(2, GetPaginationModel()->selected_page());
    EXPECT_EQ(3, GetPaginationModel()->total_pages());
  }
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       KeyboardReparentFromFolderInLastVisibleSlotUsingDownKey) {
  // Create grid with a folder on the last slot in a page (or for scrollable
  // grid, the last slot in the page with enough items that the slot is
  // initially not in the visible part of the grid).
  const int kTopLevelItemCount = paged_apps_grid_view_
                                     ? GetTilesPerPage(0) + GetTilesPerPage(1)
                                     : apps_grid_view_->cols() * 8;
  model_->PopulateApps(kTopLevelItemCount - 1);
  const AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
  apps_grid_view_->UpdatePagedViewStructure();
  UpdateLayout();

  AppListItemView* folder_view = apps_grid_view_->view_model()->view_at(
      apps_grid_view_->view_model()->view_size() - 1);
  ASSERT_TRUE(folder_view->is_folder());
  EXPECT_FALSE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      folder_view->GetBoundsInScreen()));

  folder_view->RequestFocus();
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      folder_view->GetBoundsInScreen()));
  const gfx::Rect original_folder_view_bounds =
      folder_view->GetBoundsInScreen();

  // Open the folder.
  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(original_folder_view_bounds, folder_view->GetBoundsInScreen());

  const AppListItemView* reparented_item_view =
      folder_apps_grid_view()->view_model()->view_at(0);
  ASSERT_TRUE(reparented_item_view);
  ASSERT_TRUE(reparented_item_view->item());

  std::string reparented_item_id = reparented_item_view->item()->id();
  EXPECT_EQ(base::StringPrintf("Item %d", kTopLevelItemCount - 1),
            reparented_item_id);
  ASSERT_TRUE(reparented_item_view->HasFocus());

  // Reparent the item to the slot after the folder view, which should be the
  // last spot in the grid.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_EQ(folder_item, model_->FindItem(folder_id));
  EXPECT_EQ(2u, folder_item->ChildItemCount());

  const AppListItemView* last_top_level_item_view =
      apps_grid_view_->view_model()->view_at(
          apps_grid_view_->view_model()->view_size() - 1);
  // Verify the view is within visible grid bounds, and that it has focus.
  EXPECT_EQ(reparented_item_id, last_top_level_item_view->item()->id());
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      last_top_level_item_view->GetBoundsInScreen()));
  EXPECT_TRUE(last_top_level_item_view->HasFocus());

  // In paged apps grid, the item should have been moved to a new page.
  if (paged_apps_grid_view_) {
    EXPECT_EQ(2, GetPaginationModel()->selected_page());
    EXPECT_EQ(3, GetPaginationModel()->total_pages());
  }
}

TEST_P(AppsGridViewTabletTest,
       KeyboardReparentFromFolderPrefersLeavingMovedItemOnCurrentPage) {
  model_->PopulateApps(GetTilesPerPage(0) - 2);
  const AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(3);
  model_->PopulateApps(GetTilesPerPage(1));
  const std::string folder_id = folder_item->id();
  apps_grid_view_->UpdatePagedViewStructure();
  UpdateLayout();

  AppListItemView* folder_view =
      test_api_->GetViewAtIndex(GridIndex(0, GetTilesPerPage(0) - 2));
  ASSERT_TRUE(folder_view->is_folder());
  folder_view->RequestFocus();
  const gfx::Rect original_folder_view_bounds =
      folder_view->GetBoundsInScreen();

  // Open the folder.
  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(original_folder_view_bounds, folder_view->GetBoundsInScreen());

  const AppListItemView* reparented_item_view =
      folder_apps_grid_view()->view_model()->view_at(0);
  ASSERT_TRUE(reparented_item_view);
  ASSERT_TRUE(reparented_item_view->item());

  std::string reparented_item_id = reparented_item_view->item()->id();
  EXPECT_EQ(base::StringPrintf("Item %d", GetTilesPerPage(0) - 2),
            reparented_item_id);
  ASSERT_TRUE(reparented_item_view->HasFocus());

  // Reparent the item to the slot after the folder view, which should be the
  // last spot in the grid.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_EQ(folder_item, model_->FindItem(folder_id));
  EXPECT_EQ(2u, folder_item->ChildItemCount());

  const AppListItemView* last_item_on_first_page =
      test_api_->GetViewAtIndex(GridIndex(0, GetTilesPerPage(0) - 1));
  // Verify the view is within visible grid bounds, and that it has focus.
  EXPECT_EQ(reparented_item_id, last_item_on_first_page->item()->id());
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      last_item_on_first_page->GetBoundsInScreen()));
  EXPECT_TRUE(last_item_on_first_page->HasFocus());

  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
}

// Tests that foldering the item on the last slot of a page doesn't crash.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlShiftArrowFolderLastItemOnPage) {
  const int kNumberOfApps = 4;
  model_->PopulateApps(kNumberOfApps);
  UpdateLayout();
  // Select the second to last item in the grid, folder it with the item to the
  // right.
  AppListItemView* moving_item = GetItemViewInTopLevelGrid(kNumberOfApps - 2);
  const std::string first_item_id = moving_item->item()->id();
  const std::string second_item_id =
      GetItemViewInTopLevelGrid(kNumberOfApps - 1)->item()->id();
  const gfx::Rect expected_folder_view_bounds =
      moving_item->GetBoundsInScreen();

  ui::test::EventGenerator* const event_generator = GetEventGenerator();
  // Press an arrow key to engage keyboard traversal in fullscreen launcher.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN);

  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  event_generator->PressAndReleaseKey(ui::VKEY_RIGHT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items, and that the folder is the selected view.
  AppListItemView* new_folder = GetItemViewInTopLevelGrid(kNumberOfApps - 2);
  EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));

  // With productivity launcher enabled, the folder is expected to get opened
  // after creation.
  EXPECT_EQ(!is_productivity_launcher_enabled_,
            apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(is_productivity_launcher_enabled_,
            GetAppListTestHelper()->IsInFolderView());
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
    EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                    ->GetFolderNameViewForTest()
                    ->HasFocus());

    // Close the folder.
    event_generator->PressAndReleaseKey(ui::VKEY_UP);
    event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
    EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  }
  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
}

TEST_P(AppsGridViewTabletTest, TouchDragFlipToNextPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 3 full pages of apps.
  model_->PopulateApps(GetTilesPerPage(0) + GetTilesPerPage(1) +
                       GetTilesPerPage(2));
  UpdateLayout();

  const gfx::Rect apps_grid_bounds = paged_apps_grid_view_->GetLocalBounds();
  // Drag an item to the bottom to start flipping pages.
  page_flip_waiter_->Reset();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);
  EXPECT_EQ(0, GetHapticTickEventsCount());
  gfx::Point apps_grid_bottom_center =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() - 1);
  UpdateDrag(AppsGridView::TOUCH, apps_grid_bottom_center,
             paged_apps_grid_view_, 5 /*steps*/);
  while (HasPendingPageFlip(paged_apps_grid_view_)) {
    page_flip_waiter_->Wait();
  }

  if (features::IsProductivityLauncherEnabled()) {
    // A new page cannot be created or flipped to with the ProductivityLauncher
    // flag enabled.
    EXPECT_EQ("1,2", page_flip_waiter_->selected_pages());
    EXPECT_EQ(2, GetPaginationModel()->selected_page());
  } else {
    // We flip to an extra page created at the end.
    EXPECT_EQ("1,2,3", page_flip_waiter_->selected_pages());
    EXPECT_EQ(3, GetPaginationModel()->selected_page());
  }
  // The drag is centered relative to the app item icon bounds, not the whole
  // app item view.
  gfx::Vector2d icon_offset(0,
                            GetAppListConfig()->grid_icon_bottom_padding() / 2);
  EXPECT_EQ(apps_grid_bottom_center - icon_offset, GetDragIconCenter());

  // End the drag to satisfy checks in AppsGridView destructor.
  EndDrag(apps_grid_view_, /*cancel=*/true);
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewTabletTest, DragAcrossPagesToTheLastSlot) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a full page and a partially full second page.
  model_->PopulateApps(GetTilesPerPage(0) + 3);
  apps_grid_view_->UpdatePagedViewStructure();
  UpdateLayout();

  // Drag an item from the first page to the last existing slot on the next
  // page.
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  AppListItemView* dragged_view = view_model->view_at(0);
  AppListItemView* original_first_item_on_second_page =
      view_model->view_at(GetTilesPerPage(0));

  auto* generator = GetEventGenerator();

  // Initiate drag.
  generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Drag the item to launcher page flip zone, and flip the launcher to the
  // second page.
  generator->MoveMouseTo(
      paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
      gfx::Vector2d(0, -1));
  ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));

  // Task to move mouse from the page flip area after the page gets flipped, to
  // prevent subseuquent page flips.
  PostPageFlipTask task(GetPaginationModel(), base::BindLambdaForTesting([&]() {
                          generator->MoveMouseBy(0, -50);
                          generator->MoveMouseBy(0, -50);
                        }));

  page_flip_waiter_->Wait();

  // Ensure that the reoreder timer ran, and that any views on the second page
  // that should have been moved to the first page have done so.
  ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
  paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
  test_api_->WaitForItemMoveAnimationDone();

  // Move the item to the first empty slot on the second page.
  gfx::Point empty_slot =
      test_api_->GetItemTileRectAtVisualIndex(1, 3).CenterPoint();
  views::View::ConvertPointToScreen(paged_apps_grid_view_, &empty_slot);
  generator->MoveMouseTo(empty_slot);
  test_api_->WaitForItemMoveAnimationDone();

  if (paged_apps_grid_view_->reorder_timer_for_test()->IsRunning())
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
  test_api_->WaitForItemMoveAnimationDone();

  const int expected_final_slot = is_productivity_launcher_enabled_ ? 2 : 3;
  EXPECT_EQ(GridIndex(1, expected_final_slot),
            paged_apps_grid_view_->reorder_placeholder());

  // Verify that the last item in the grid is left of the expected placeholder
  // location.
  const gfx::Rect last_slot_rect =
      GetItemRectOnCurrentPageAt(1, expected_final_slot);
  const views::View* last_view =
      view_model->view_at(view_model->view_size() - 1);
  if (is_rtl_) {
    gfx::Point last_view_left_center_in_grid =
        last_view->GetLocalBounds().left_center();
    views::View::ConvertPointToTarget(last_view, paged_apps_grid_view_,
                                      &last_view_left_center_in_grid);
    EXPECT_GE(last_view_left_center_in_grid.x(), last_slot_rect.right());
  } else {
    gfx::Point last_view_right_center_in_grid =
        last_view->GetLocalBounds().right_center();
    views::View::ConvertPointToTarget(last_view, paged_apps_grid_view_,
                                      &last_view_right_center_in_grid);
    EXPECT_LE(last_view_right_center_in_grid.x(), last_slot_rect.x());
  }

  EndDrag(paged_apps_grid_view_, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the last slot.
  AppListItemView* last_item_view =
      test_api_->GetViewAtVisualIndex(1, expected_final_slot);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(dragged_view->item()->id(), last_item_view->item()->id());

  // For productivity launcher, the first item on second page should have been
  // moved to the first page (to fill up the empty slot left by moving the
  // draggged item away). For non-productivity launcher, the last slot should
  // remain empty.
  AppListItemView* last_item_on_first_page =
      test_api_->GetViewAtVisualIndex(0, GetTilesPerPage(0) - 1);
  ASSERT_EQ(is_productivity_launcher_enabled_, !!last_item_on_first_page);
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(original_first_item_on_second_page->item()->id(),
              last_item_on_first_page->item()->id());
  }
}

TEST_P(AppsGridViewTabletTest, DragAcrossPagesToSecondToLastSlot) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a full page and a partially full second page.
  model_->PopulateApps(GetTilesPerPage(0) + 3);
  apps_grid_view_->UpdatePagedViewStructure();
  UpdateLayout();

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  AppListItemView* dragged_view = view_model->view_at(0);
  AppListItemView* original_first_item_on_second_page =
      view_model->view_at(GetTilesPerPage(0));

  auto* generator = GetEventGenerator();

  // Initiate drag.
  generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Drag the item to launcher page flip zone, and flip the launcher to the
  // second page.
  generator->MoveMouseTo(
      paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
      gfx::Vector2d(0, -1));
  ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));

  // Task to move mouse from the page flip area after the page gets flipped, to
  // prevent subseuquent page flips.
  PostPageFlipTask task(GetPaginationModel(), base::BindLambdaForTesting([&]() {
                          generator->MoveMouseBy(0, -50);
                          generator->MoveMouseBy(0, -50);
                        }));

  page_flip_waiter_->Wait();

  // Ensure that the reoreder timer ran, and that any views on the second page
  // that should have been moved to the first page have done so.
  ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
  paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
  test_api_->WaitForItemMoveAnimationDone();

  // Move the item between two last slots on the page.
  views::View* last_view = view_model->view_at(view_model->view_size() - 1);
  const gfx::Point last_slot = last_view->GetBoundsInScreen().CenterPoint();
  views::View* second_to_last_view =
      view_model->view_at(view_model->view_size() - 2);
  const gfx::Point second_to_last_slot =
      second_to_last_view->GetBoundsInScreen().CenterPoint();
  const gfx::Point drop_point((last_slot.x() + second_to_last_slot.x()) / 2,
                              last_slot.y());

  generator->MoveMouseTo(drop_point);

  if (paged_apps_grid_view_->reorder_timer_for_test()->IsRunning())
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
  test_api_->WaitForItemMoveAnimationDone();

  const int expected_final_slot = is_productivity_launcher_enabled_ ? 1 : 2;
  EXPECT_EQ(GridIndex(1, expected_final_slot),
            paged_apps_grid_view_->reorder_placeholder());

  // Verify that the last item in the grid is right of the expected placeholder
  // location.
  const gfx::Rect target_slot_rect =
      GetItemRectOnCurrentPageAt(1, expected_final_slot);
  if (is_rtl_) {
    gfx::Point last_view_right_center_in_grid =
        last_view->GetLocalBounds().right_center();
    views::View::ConvertPointToTarget(last_view, paged_apps_grid_view_,
                                      &last_view_right_center_in_grid);
    EXPECT_LE(last_view_right_center_in_grid.x(), target_slot_rect.x());
  } else {
    gfx::Point last_view_left_center_in_grid =
        last_view->GetLocalBounds().left_center();
    views::View::ConvertPointToTarget(last_view, paged_apps_grid_view_,
                                      &last_view_left_center_in_grid);
    EXPECT_GE(last_view_left_center_in_grid.x(), target_slot_rect.right());
  }

  // Verify that second to last item in the grid is left of the expected
  // placeholder location.
  if (is_rtl_) {
    gfx::Point second_to_last_view_left_center_in_grid =
        second_to_last_view->GetLocalBounds().left_center();
    views::View::ConvertPointToTarget(second_to_last_view,
                                      paged_apps_grid_view_,
                                      &second_to_last_view_left_center_in_grid);
    EXPECT_GE(second_to_last_view_left_center_in_grid.x(),
              target_slot_rect.right());
  } else {
    gfx::Point second_to_last_view_right_center_in_grid =
        second_to_last_view->GetLocalBounds().right_center();
    views::View::ConvertPointToTarget(
        second_to_last_view, paged_apps_grid_view_,
        &second_to_last_view_right_center_in_grid);
    EXPECT_LE(second_to_last_view_right_center_in_grid.x(),
              target_slot_rect.x());
  }

  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the target slot.
  AppListItemView* last_item_view =
      test_api_->GetViewAtVisualIndex(1, expected_final_slot);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(dragged_view->item()->id(), last_item_view->item()->id());

  // For productivity launcher, the first item on second page should have been
  // moved to the first page (to fill up the empty slot left by moving the
  // draggged item away). For non-productivity launcher, the last slot should
  // remain empty.
  AppListItemView* last_item_on_first_page =
      test_api_->GetViewAtVisualIndex(0, GetTilesPerPage(0) - 1);
  ASSERT_EQ(is_productivity_launcher_enabled_, !!last_item_on_first_page);
  if (is_productivity_launcher_enabled_) {
    EXPECT_EQ(original_first_item_on_second_page->item()->id(),
              last_item_on_first_page->item()->id());
  }
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeOverflownLandspaceToPortait) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const int kTotalApps = GetTilesPerPage(0) + GetTilesPerPage(1) + 1;
  model_->PopulateApps(kTotalApps);
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages decreased if new
  // page structure fit all apps into 2 pages (number of items per page may
  // change between landscape and protrait mode for productivity launcher).
  UpdateDisplay("1024x768/r");

  EXPECT_EQ(kTotalApps <= GetTilesPerPage(0) + GetTilesPerPage(1) ? 2 : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeUnderflowLandspaceToPortait) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const int kTotalApps = GetTilesPerPage(0) + GetTilesPerPage(1) - 1;
  model_->PopulateApps(kTotalApps);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages increased if new
  // page structure does not fit all apps into 2 pages (number of items per page
  // may change between landscape and protrait mode for productivity launcher).
  UpdateDisplay("1024x768/r");

  EXPECT_EQ(kTotalApps <= GetTilesPerPage(0) + GetTilesPerPage(1) ? 2 : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeOverflownPortraitToLandcape) {
  ASSERT_TRUE(paged_apps_grid_view_);
  UpdateDisplay("1024x768/r");

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const int kTotalApps = GetTilesPerPage(0) + GetTilesPerPage(1) + 1;
  model_->PopulateApps(kTotalApps);
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages decreased if new
  // page structure fit all apps into 2 pages (number of items per page may
  // change between landscape and protrait mode for productivity launcher).
  UpdateDisplay("1024x768");

  EXPECT_EQ(kTotalApps <= GetTilesPerPage(0) + GetTilesPerPage(1) ? 2 : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeUnderflowPortraitToLandscape) {
  ASSERT_TRUE(paged_apps_grid_view_);
  UpdateDisplay("1024x768/r");

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const int kTotalApps = GetTilesPerPage(0) + GetTilesPerPage(1) - 1;
  model_->PopulateApps(kTotalApps);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages increaesed if new
  // page structure fits all apps into 2 pages (which may be the case if
  // productivity launcher is enabled, in which case protrait mode grid has more
  // items per page than landscape UI).
  UpdateDisplay("1024x768");

  EXPECT_EQ(kTotalApps <= GetTilesPerPage(0) + GetTilesPerPage(1) ? 2 : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest, TouchDragFlipToPreviousPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 3 full pages of apps.
  model_->PopulateApps(GetTilesPerPage(0) + GetTilesPerPage(1) +
                       GetTilesPerPage(2));
  // Select the last page.
  GetPaginationModel()->SelectPage(2, /*animate=*/false);

  // Drag an item to the top to start flipping pages.
  page_flip_waiter_->Reset();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);
  EXPECT_EQ(0, GetHapticTickEventsCount());
  gfx::Point apps_grid_top_center(
      paged_apps_grid_view_->GetLocalBounds().width() / 2, 0);
  UpdateDrag(AppsGridView::TOUCH, apps_grid_top_center, paged_apps_grid_view_,
             5 /*steps*/);
  while (HasPendingPageFlip(paged_apps_grid_view_)) {
    page_flip_waiter_->Wait();
  }

  // We flipped back to the first page.
  EXPECT_EQ("1,0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  // The drag is centered relative to the app item icon bounds, not the whole
  // app item view.
  gfx::Vector2d icon_offset(0,
                            GetAppListConfig()->grid_icon_bottom_padding() / 2);
  EXPECT_EQ(apps_grid_top_center - icon_offset, GetDragIconCenter());

  // End the drag to satisfy checks in AppsGridView destructor.
  EndDrag(paged_apps_grid_view_, /*cancel=*/true);
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, CancelDragDoesNotReorderItems) {
  const int kTotalItems = 4;
  model_->PopulateApps(kTotalItems);
  ASSERT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  // Starts a mouse drag and then cancels it.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  const gfx::Point to = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_);
  EndDrag(apps_grid_view_, /*cancel=*/true);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Model is not changed.
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());
}

// Test focus change before dragging an item. (See https://crbug.com/834682)
TEST_F(AppsGridViewTest, FocusOfDraggedViewBeforeDrag) {
  model_->PopulateApps(1);
  UpdateLayout();
  EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
  EXPECT_FALSE(apps_grid_view_->view_model()->view_at(0)->HasFocus());
}

// Test focus change during dragging an item. (See https://crbug.com/834682)
TEST_P(AppsGridViewDragTest, FocusOfDraggedViewDuringDrag) {
  model_->PopulateApps(1);
  UpdateLayout();
  AppListItemView* item_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::MOUSE, 0, 0, apps_grid_view_);
  const gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  // Dragging the item towards its right.
  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);

  EXPECT_FALSE(search_box_view_->search_box()->HasFocus());
  EXPECT_TRUE(item_view->HasFocus());

  EndDrag(apps_grid_view_, false /*cancel*/);
}

// Test focus change after dragging an item. (See https://crbug.com/834682)
TEST_P(AppsGridViewDragTest, FocusOfDraggedViewAfterDrag) {
  model_->PopulateApps(1);
  UpdateLayout();
  auto* item_view = apps_grid_view_->view_model()->view_at(0);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  const gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  EndDrag(apps_grid_view_, false /*cancel*/);

  if (features::IsProductivityLauncherEnabled()) {
    // ProductivityLauncher keeps focus on the search box after drags.
    EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
    EXPECT_FALSE(item_view->HasFocus());
  } else {
    EXPECT_FALSE(search_box_view_->search_box()->HasFocus());
    EXPECT_TRUE(item_view->HasFocus());
  }
}

// Verify the dragged item's focus after the item is dragged from a folder with
// a single items.
TEST_P(AppsGridViewDragTest, FocusOfReparentedDragViewWithFolderDeleted) {
  // Creates a folder item with two items.
  model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(1);
  test_api_->Update();

  // Leave the dragged item as a single folder child.
  model_->DeleteItem("Item 1");
  // One folder and one app. Therefore the top level view count is 2.
  EXPECT_EQ(2, apps_grid_view_->view_model()->view_size());

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag the first folder child out of the folder.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     folder_apps_grid_view());
  gfx::Point point_outside_folder =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(10, 10);
  UpdateDrag(AppsGridView::MOUSE, point_outside_folder, folder_apps_grid_view(),
             /*steps=*/10);

  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Drop the item in (0,2) spot is the root apps grid. The spot is expected to
  // be empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             /*steps=*/5);
  BoundsChangeCounter counter(GetItemViewInTopLevelGrid(1));
  EndDrag(folder_apps_grid_view(), /*cancel=*/false);

  // The folder should be deleted. The first item should be Item 2, the second
  // item should be Item 0.
  EXPECT_EQ(2, apps_grid_view_->view_model()->view_size());
  EXPECT_EQ("Item 2", GetItemViewInTopLevelGrid(0)->item()->id());
  EXPECT_EQ("Item 0", GetItemViewInTopLevelGrid(1)->item()->id());

  AppListItemView* const dragged_view = GetItemViewInTopLevelGrid(1);
  if (features::IsProductivityLauncherEnabled()) {
    // Verify that Item 2's bounds do not change after calling `EndDrag()`.
    EXPECT_EQ(0, counter.bounds_change_count());

    // ProductivityLauncher keeps focus on the search box after drags.
    EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
    EXPECT_FALSE(dragged_view->HasFocus());
  } else {
    // Verify that Item 2's bounds change once after calling `EndDrag()` due to
    // ending the cardified state.
    EXPECT_EQ(1, counter.bounds_change_count());

    // The dragged item is focused but is not selected.
    EXPECT_TRUE(dragged_view->HasFocus());
    EXPECT_FALSE(apps_grid_view_->has_selected_view());

    // Press the arrow key. The dragged item is selected now.
    PressAndReleaseKey(ui::VKEY_RIGHT);
    EXPECT_TRUE(dragged_view->HasFocus());
    EXPECT_TRUE(apps_grid_view_->has_selected_view());
    EXPECT_EQ(dragged_view, apps_grid_view_->selected_view());
  }
}

TEST_P(AppsGridViewDragTest, FocusOfReparentedDragViewAfterDrag) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  model_->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  model_->PopulateApps(2);
  test_api_->Update();

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag the first folder child out of the folder.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     folder_apps_grid_view());
  gfx::Point point_outside_folder =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(10, 10);
  UpdateDrag(AppsGridView::MOUSE, point_outside_folder, folder_apps_grid_view(),
             /*steps=*/10);

  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Drop the item in (0,3) spot is the root apps grid. The spot is expected to
  // be empty.
  gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
  views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                    &drop_point);
  UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
             /*steps=*/5);
  EndDrag(folder_apps_grid_view(), /*cancel=*/false);

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(3);
  EXPECT_EQ("Item 0", item_view->item()->id());

  if (features::IsProductivityLauncherEnabled()) {
    // ProductivityLauncher keeps focus on the search box after drags.
    EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
    EXPECT_FALSE(item_view->HasFocus());
  } else {
    EXPECT_TRUE(item_view->HasFocus());
  }
}

TEST_P(AppsGridViewDragTest, DragAndPinItemToShelf) {
  model_->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify that item drag has started.
  ASSERT_TRUE(apps_grid_view_->drag_item());
  ASSERT_TRUE(apps_grid_view_->IsDragging());
  ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(apps_grid_view_->FireDragToShelfTimerForTest());

  EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);

  // Releasing drag over shelf should pin the dragged app.
  generator->ReleaseLeftButton();
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_EQ("Item 1", ShelfModel::Get()->items()[0].id.app_id);
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewDragTest, DragAndPinNotInitiallyVisibleItemToShelf) {
  // Add more apps to the root apps grid.
  model_->PopulateApps(50);
  UpdateLayout();

  // Select item that is not withing the default apps grid view bounds.
  AppListItemView* const item_view = GetItemViewInTopLevelGrid(40);
  ASSERT_FALSE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      item_view->GetBoundsInScreen()));

  // Focusing and scrolling view to visible should ensure it becomes visible
  // both in scrollable and paged apps grid.
  item_view->RequestFocus();
  item_view->ScrollViewToVisible();

  ASSERT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      item_view->GetBoundsInScreen()));

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify app list item drag has started.
  ASSERT_TRUE(apps_grid_view_->drag_item());
  ASSERT_TRUE(apps_grid_view_->IsDragging());
  ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(apps_grid_view_->FireDragToShelfTimerForTest());

  EXPECT_EQ("Item 40", shelf_view->drag_and_drop_shelf_id().app_id);

  // Releasing drag over shelf should pin the dragged app.
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 40"));
  EXPECT_EQ("Item 40", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragItemToAndFromShelf) {
  model_->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify app list item drag has started.
  ASSERT_TRUE(apps_grid_view_->drag_item());
  ASSERT_TRUE(apps_grid_view_->IsDragging());
  ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(apps_grid_view_->FireDragToShelfTimerForTest());
  EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);

  // Move the app away from shelf, and verify the app doesn't get pinned when
  // the drag ends.
  generator->MoveMouseTo(apps_grid_view_->GetBoundsInScreen().origin());
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_TRUE(ShelfModel::Get()->items().empty());
}

TEST_P(AppsGridViewDragTest, DragAndPinItemFromFolderToShelf) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  model_->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  model_->PopulateApps(2);
  test_api_->Update();

  // Open the folder.
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(1, folder_apps_grid_view());

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify app list item drag has started.
  ASSERT_TRUE(folder_apps_grid_view()->drag_item());
  ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
  ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

  generator->MoveMouseTo(
      app_list_folder_view()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(20, 0));

  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(folder_apps_grid_view()->FireDragToShelfTimerForTest());

  EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);

  // Releasing drag over shelf should pin the dragged app.
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_EQ("Item 1", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragAndPinNotInitiallyVisibleFolderItemToShelf) {
  model_->CreateAndPopulateFolderWithApps(2 * kMaxItemsPerFolderPage);
  UpdateLayout();

  // Open the folder.
  test_api_->PressItemAt(0);

  // Select item that is not within the initial folder view bounds.
  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(30, folder_apps_grid_view());

  ASSERT_FALSE(app_list_folder_view()->GetBoundsInScreen().Contains(
      item_view->GetBoundsInScreen()));

  // Focusing and scrolling view to visible should ensure it becomes visible
  // both in scrollable and paged apps grid.
  item_view->RequestFocus();
  item_view->ScrollViewToVisible();

  ASSERT_TRUE(app_list_folder_view()->GetBoundsInScreen().Contains(
      item_view->GetBoundsInScreen()));

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify app list item drag has started.
  ASSERT_TRUE(folder_apps_grid_view()->drag_item());
  ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
  ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

  generator->MoveMouseTo(
      app_list_folder_view()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(20, 0));

  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(folder_apps_grid_view()->FireDragToShelfTimerForTest());

  EXPECT_EQ("Item 30", shelf_view->drag_and_drop_shelf_id().app_id);

  // Releasing drag over shelf should pin the dragged app.
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 30"));
  EXPECT_EQ("Item 30", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragAnItemFromFolderToAndFromShelf) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  model_->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  model_->PopulateApps(2);
  UpdateLayout();

  // Open the folder.
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(1, folder_apps_grid_view());

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item_view->FireMouseDragTimerForTest();
  generator->MoveMouseBy(10, 10);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  // Verify app list item drag has started.
  ASSERT_TRUE(folder_apps_grid_view()->drag_item());
  ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
  ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

  generator->MoveMouseTo(
      app_list_folder_view()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(20, 0));

  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  // Shelf should start handling the drag if it moves within its bounds.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  generator->MoveMouseTo(shelf_view->GetBoundsInScreen().left_center());
  ASSERT_TRUE(folder_apps_grid_view()->FireDragToShelfTimerForTest());

  EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);

  // Move the app away from shelf, and verify the app doesn't get pinned when
  // the drag ends.
  generator->MoveMouseTo(apps_grid_view_->GetBoundsInScreen().origin());
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_TRUE(ShelfModel::Get()->items().empty());
}

TEST_P(AppsGridViewTabletTest, Basic) {
  base::HistogramTester histogram_tester;

  model_->PopulateApps(GetTilesPerPage(0) + 1);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, -1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, -10));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  // Drag up on the app grid when on page 1, this should move the AppsGridView
  // but not the AppListView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 0);

  apps_grid_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_FALSE(app_list_view_->is_in_drag());
  ASSERT_NE(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "TabletMode",
      0);

  apps_grid_view_->OnGestureEvent(&scroll_end);

  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "TabletMode",
      1);
}

// Make sure that a folder icon resets background blur after scrolling the
// apps grid without completing any transition (See
// https://crbug.com/1049275). The background blur is masked by the apps
// grid's layer mask.
TEST_P(AppsGridViewTabletTest, EnsureBlurAfterScrollingWithoutTransition) {
  // Create a folder with 2 apps. Then add apps until a second page is
  // created.
  model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(GetTilesPerPage(0));
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, -1));
  ui::GestureEvent scroll_update_upwards(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, -10));
  ui::GestureEvent scroll_update_downwards(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, 15));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  AppListItemView* folder_view = GetItemViewInTopLevelGrid(0);
  ASSERT_TRUE(folder_view->is_folder());

  views::View* scrollable_container = app_list_view_->app_list_main_view()
                                          ->contents_view()
                                          ->apps_container_view()
                                          ->scrollable_container_for_test();
  ASSERT_FALSE(scrollable_container->layer()->layer_mask_layer());

  // On the first page drag upwards, there should not be a page switch and the
  // layer mask should make the folder lose blur.
  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  apps_grid_view_->OnGestureEvent(&scroll_update_upwards);
  EXPECT_TRUE(scroll_update_upwards.handled());

  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  ASSERT_TRUE(scrollable_container->layer()->layer_mask_layer());

  // Continue drag, now switching directions and release. There shouldn't be
  // any transition and the mask layer should've been reset.
  apps_grid_view_->OnGestureEvent(&scroll_update_downwards);
  EXPECT_TRUE(scroll_update_downwards.handled());
  apps_grid_view_->OnGestureEvent(&scroll_end);
  EXPECT_TRUE(scroll_end.handled());

  EXPECT_FALSE(GetPaginationModel()->has_transition());
  EXPECT_FALSE(scrollable_container->layer()->layer_mask_layer());
}

TEST_F(AppsGridViewTest, PopulateAppsGridWithTwoApps) {
  const int kApps = 2;
  model_->PopulateApps(kApps);

  // There's only one page and both items are in that page.
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(2, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(0)->item()->id());
  EXPECT_EQ(view_model->view_at(1),
            test_api_->GetViewAtVisualIndex(0 /* page */, 1 /* slot */));
  EXPECT_EQ("Item 1", view_model->view_at(1)->item()->id());
  EXPECT_EQ(std::string("Item 0,Item 1"), model_->GetModelContent());
}

TEST_F(AppsGridViewTest, PopulateAppsGridWithAFolder) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsPerFolderPage;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);

  // Open the folder and check it's contents.
  test_api_->Update();
  test_api_->PressItemAt(0);

  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());
  EXPECT_EQ(4, folder_apps_grid_view()->cols());
  EXPECT_EQ(16, AppsGridViewTestApi(folder_apps_grid_view()).TilesPerPage(0));
  EXPECT_EQ(1, GetTotalPages(folder_apps_grid_view()));
  EXPECT_EQ(0, GetSelectedPage(folder_apps_grid_view()));
  EXPECT_TRUE(folder_apps_grid_view()->IsInFolder());
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, MoveAnItemToNewEmptyPage) {
  const int kApps = 2;
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();

  // Drag the first item to the page bottom.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);

  // A new second page is created and the first item is put on it.
  EXPECT_EQ("1", page_flip_waiter_->selected_pages());
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();
  EXPECT_EQ(2, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 1", view_model->view_at(0)->item()->id());
  EXPECT_EQ(view_model->view_at(1),
            test_api_->GetViewAtVisualIndex(1 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(1)->item()->id());
  EXPECT_EQ(std::string("Item 1,PageBreakItem,Item 0"),
            model_->GetModelContent());
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, MoveLastItemToCreateFolderInNextPage) {
  const int kApps = 2;
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();

  // Drag the first item to next page and drag the second item to overlap with
  // the first item.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(2, GetHapticTickEventsCount());
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);

  // A new folder is created on second page, but since the first page is
  // empty, the page is removed and the new folder ends up on first page.
  EXPECT_EQ(1, GetPaginationModel()->total_pages());
  EXPECT_EQ("1,0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(1, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  const AppListItem* folder_item = view_model->view_at(0)->item();
  EXPECT_TRUE(folder_item->is_folder());
  // The "page break" item remains, but it will be removed later in
  // AppListSyncableService.
  EXPECT_EQ(std::string("PageBreakItem," + folder_item->id()),
            model_->GetModelContent());
  EXPECT_EQ(2, GetHapticTickEventsCount());
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, MoveLastItemForReorderInNextPage) {
  const int kApps = 2;
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(1, 0);
  gfx::Point to_in_next_page = tile_rect.CenterPoint();
  to_in_next_page.set_x(tile_rect.x());

  // Drag the first item to next page and drag the second item to the left of
  // the first item.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(2, GetHapticTickEventsCount());
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);

  // The second item is put on the left of the first item, but since the first
  // page is empty, the page is removed and both items end up on first page.
  EXPECT_EQ("1,0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(2, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 1", view_model->view_at(0)->item()->id());
  EXPECT_EQ(view_model->view_at(1),
            test_api_->GetViewAtVisualIndex(0 /* page */, 1 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(1)->item()->id());
  // The "page break" item remains, but it will be removed later in
  // AppListSyncableService.
  EXPECT_EQ(std::string("PageBreakItem,Item 1,Item 0"),
            model_->GetModelContent());
  EXPECT_EQ(2, GetHapticTickEventsCount());
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, MoveLastItemToNewEmptyPage) {
  const int kApps = 1;
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();

  // Drag the item to next page.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     apps_grid_view_);
  EXPECT_EQ(2, GetHapticTickEventsCount());
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);

  // The item is put on second page, but since the first page is empty,
  // the page is removed and the item ends up on first page.
  EXPECT_EQ("1,0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(1, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(0)->item()->id());
  EXPECT_EQ(std::string("Item 0"), model_->GetModelContent());
  EXPECT_EQ(2, GetHapticTickEventsCount());
}

// There's no "page break" item at the end of first page with full grid.
TEST_F(AppsGridViewTest, NoPageBreakItemWithFullGrid) {
  // There are two pages and last item is on second page.
  const int kApps = 2 + GetTilesPerPage(0);
  model_->PopulateApps(kApps);
  std::string model_content = "Item 0";
  for (int i = 1; i < kApps; ++i)
    model_content.append(",Item " + base::NumberToString(i));

  EXPECT_EQ(model_content, model_->GetModelContent());
}

TEST_P(AppsGridViewClamshellAndTabletTest, RootGridUpdatesOnModelChange) {
  model_->PopulateApps(2);
  UpdateLayout();

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(2, view_model->view_size());
  TestAppListItemViewIndice();

  // Update the model, and verify the apps grid gets updated.
  auto model_override = std::make_unique<test::AppListTestModel>();
  model_override->PopulateApps(3);
  model_override->CreateAndPopulateFolderWithApps(5);
  model_override->PopulateApps(3);

  auto search_model_override = std::make_unique<SearchModel>();

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get());
  UpdateLayout();

  // Verify that the view model size matches the new model.
  EXPECT_EQ(7, view_model->view_size());
  TestAppListItemViewIndice();

  // Verify that clicking an item activates it.
  LeftClickOn(view_model->view_at(0));
  EXPECT_EQ("Item 0", GetTestAppListClient()->activate_item_last_id());

  // Clicking on the folder item transitions to folder view.
  LeftClickOn(view_model->view_at(3));
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  ASSERT_EQ(5, folder_apps_grid_view()->view_model()->view_size());

  // Click on an item within the folder.
  LeftClickOn(folder_apps_grid_view()->view_model()->view_at(1));
  EXPECT_EQ("Item 4", GetTestAppListClient()->activate_item_last_id());

  // Switch model to original one, and verify the folder view gets closed.
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_.get(), search_model_.get());
  UpdateLayout();
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(2, view_model->view_size());
  TestAppListItemViewIndice();

  LeftClickOn(view_model->view_at(1));
  EXPECT_EQ("Item 1", GetTestAppListClient()->activate_item_last_id());

  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(0, view_model->view_size());
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       TouchScrollFromFolderNameDoesNotAffectRootGrid) {
  // Add enough items to the root grid so the launcher becomes paged.
  model_->PopulateApps(1);
  model_->CreateAndPopulateFolderWithApps(5);
  // `GetTilesPerPage()` may return a large number for bubble launcher - ensure
  // the number of test apps is not excessive.
  model_->PopulateApps(std::min(30, GetTilesPerPage(0)));
  UpdateLayout();

  // Open the folder view.
  LeftClickOn(apps_grid_view_->view_model()->view_at(1));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  AppsGridView* const root_grid_view = apps_grid_view_;
  const gfx::Point original_root_grid_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  const gfx::Point original_first_item_origin =
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin();
  ui::test::ScrollStepCallback verify_grid_bounds = base::BindLambdaForTesting(
      [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
        EXPECT_EQ(original_root_grid_origin,
                  root_grid_view->GetBoundsInScreen().origin());
        EXPECT_EQ(original_first_item_origin, root_grid_view->view_model()
                                                  ->view_at(0)
                                                  ->GetBoundsInScreen()
                                                  .origin());
      });

  // Simulate upward gesture scroll from folder header view, and verify it
  // doesn't affect the root apps grid view location.
  gfx::Point scroll_start = app_list_folder_view_->folder_header_view()
                                ->GetBoundsInScreen()
                                .CenterPoint();
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      scroll_start, scroll_start - gfx::Vector2d(0, 100),
      /*duration=*/base::Milliseconds(50),
      /*steps=*/5, verify_grid_bounds);

  ASSERT_EQ(original_root_grid_origin,
            apps_grid_view_->GetBoundsInScreen().origin());
  ASSERT_EQ(
      original_first_item_origin,
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin());
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Simulate downward gesture scroll from folder header view, and verify it
  // doesn't affect the root apps grid view location.
  scroll_start = app_list_folder_view_->folder_header_view()
                     ->GetBoundsInScreen()
                     .CenterPoint();
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      scroll_start, scroll_start + gfx::Vector2d(0, 100),
      /*duration=*/base::Milliseconds(50),
      /*steps=*/5, verify_grid_bounds);

  EXPECT_EQ(original_root_grid_origin,
            apps_grid_view_->GetBoundsInScreen().origin());
  EXPECT_EQ(
      original_first_item_origin,
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin());
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       TouchScrollFromFolderGridDoesNotAffectRootGrid) {
  // Add enough items to the root grid so the launcher becomes paged.
  model_->PopulateApps(1);
  model_->CreateAndPopulateFolderWithApps(5);
  // `GetTilesPerPage()` may return a large number for bubble launcher - ensure
  // the number of test apps is not excessive.
  model_->PopulateApps(std::min(30, GetTilesPerPage(0)));
  UpdateLayout();

  // Open the folder view.
  LeftClickOn(apps_grid_view_->view_model()->view_at(1));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  AppsGridView* const root_grid_view = apps_grid_view_;
  const gfx::Point original_root_grid_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  const gfx::Point original_first_item_origin =
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin();
  ui::test::ScrollStepCallback verify_grid_bounds = base::BindLambdaForTesting(
      [&](ui::EventType event, const gfx::Vector2dF& offset) {
        EXPECT_EQ(original_root_grid_origin,
                  root_grid_view->GetBoundsInScreen().origin());
        EXPECT_EQ(original_first_item_origin, root_grid_view->view_model()
                                                  ->view_at(0)
                                                  ->GetBoundsInScreen()
                                                  .origin());
      });

  // Simulate downward gesture scroll from folder grid (outside any folder app
  // list item view), and verify it doesn't affect the root apps grid view
  // location.
  gfx::Point scroll_start = GetItemViewInAppsGridAt(0, folder_apps_grid_view())
                                ->GetBoundsInScreen()
                                .right_center() +
                            gfx::Vector2d(1, 0);
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      scroll_start, scroll_start - gfx::Vector2d(0, 100),
      /*duration=*/base::Milliseconds(50),
      /*steps=*/5, verify_grid_bounds);

  ASSERT_EQ(original_root_grid_origin,
            apps_grid_view_->GetBoundsInScreen().origin());
  ASSERT_EQ(
      original_first_item_origin,
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin());
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Simulate downward gesture scroll from folder header view, and verify it
  // doesn't affect the root apps grid view location.
  scroll_start = GetItemViewInAppsGridAt(0, folder_apps_grid_view())
                     ->GetBoundsInScreen()
                     .right_center() +
                 gfx::Vector2d(1, 0);
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      scroll_start, scroll_start + gfx::Vector2d(0, 100),
      /*duration=*/base::Milliseconds(50),
      /*steps=*/5, verify_grid_bounds);

  EXPECT_EQ(original_root_grid_origin,
            apps_grid_view_->GetBoundsInScreen().origin());
  EXPECT_EQ(
      original_first_item_origin,
      apps_grid_view_->view_model()->view_at(0)->GetBoundsInScreen().origin());
}

// This is a NonBubble test because page breaks are ignored with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, PageBreakItemAddedAfterDrag) {
  // There are two pages and last item is on second page.
  const int kApps = 2 + GetTilesPerPage(0);
  model_->PopulateApps(kApps);
  GetPaginationModel()->SelectPage(1, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(0, 0);
  gfx::Point to_in_previous_page =
      is_rtl_ ? tile_rect.right_center() : tile_rect.left_center();

  // Drag the last item to the first item's left position in previous page.
  UpdateDragToNeighborPage(false /* next_page */, to_in_previous_page);

  // A "page break" item is added to split the pages.
  std::string model_content = "Item " + base::NumberToString(kApps - 1);
  for (int i = 1; i < kApps; ++i) {
    model_content.append(",Item " + base::NumberToString(i - 1));
    if (i == GetTilesPerPage(0) - 1)
      model_content.append(",PageBreakItem");
  }
  EXPECT_EQ(model_content, model_->GetModelContent());
}

TEST_P(AppsGridViewTabletTest, MoveItemToPreviousFullPage) {
  // There are two pages and last item is on second page.
  const int kApps = 2 + GetTilesPerPage(0);
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  GetPaginationModel()->SelectPage(1, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());

  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(0, 0);
  gfx::Point to_in_previous_page =
      is_rtl_ ? tile_rect.right_center() : tile_rect.left_center();

  // Drag the last item to the first item's left position in previous
  // page.
  UpdateDragToNeighborPage(false /* next_page */, to_in_previous_page);

  // The dragging is successful, the last item becomes the first item.
  EXPECT_EQ("0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(kApps, view_model->view_size());
  for (int i = 0; i < kApps; ++i) {
    int page = i / GetTilesPerPage(0);
    int slot = i % GetTilesPerPage(0);
    EXPECT_EQ(view_model->view_at(i),
              test_api_->GetViewAtVisualIndex(page, slot));
    EXPECT_EQ("Item " + base::NumberToString((i + kApps - 1) % kApps),
              view_model->view_at(i)->item()->id());
  }
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// This is a NonBubble test because page breaks are ignored with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, MoveItemSubsequentDragKeepPageBreak) {
  // There are two pages and last item is on second page.
  const int kApps = 2 + GetTilesPerPage(0);
  model_->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  GetPaginationModel()->SelectPage(1, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);

  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(0, 0);
  gfx::Point to_in_previous_page =
      is_rtl_ ? tile_rect.right_center() : tile_rect.left_center();

  // Drag the last item to the first item's left position in previous
  // page twice.
  UpdateDragToNeighborPage(false /* next_page */, to_in_previous_page);
  // Again drag the last item to the first item's left position in previous
  // page.
  GetPaginationModel()->SelectPage(1, false);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 1,
                                     apps_grid_view_);
  UpdateDragToNeighborPage(false /* next_page */, to_in_previous_page);

  // The dragging is successful, the last item becomes the first item again.
  EXPECT_EQ("0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(kApps, view_model->view_size());
  for (int i = 0; i < kApps; ++i) {
    int page = i / GetTilesPerPage(0);
    int slot = i % GetTilesPerPage(0);
    EXPECT_EQ(view_model->view_at(i),
              test_api_->GetViewAtVisualIndex(page, slot));
    EXPECT_EQ("Item " + base::NumberToString((i + kApps - 2) % kApps),
              view_model->view_at(i)->item()->id());
  }
  // A "page break" item still exists.
  std::string model_content = "Item " + base::NumberToString(kApps - 2) +
                              ",Item " + base::NumberToString(kApps - 1);
  for (int i = 2; i < kApps; ++i) {
    model_content.append(",Item " + base::NumberToString(i - 2));
    if (i == GetTilesPerPage(0) - 1)
      model_content.append(",PageBreakItem");
  }
  EXPECT_EQ(model_content, model_->GetModelContent());
}

TEST_F(AppsGridViewTest, CreateANewPageWithKeyboardLogsMetrics) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(2);

  // Select first app and move it with the keyboard down to create a new page.
  AppListItemView* moving_item = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  ASSERT_EQ(GetPaginationModel()->total_pages(), 2);
  histogram_tester.ExpectBucketCount(
      "Apps.AppList.AppsGridAddPage",
      AppListPageCreationType::kMovingAppWithKeyboard, 1);
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest, CreateANewPageByDraggingLogsMetrics) {
  ASSERT_TRUE(paged_apps_grid_view_) << "Only available in tablet mode or when "
                                        "ProductivityLauncher is disabled.";

  base::HistogramTester histogram_tester;
  model_->PopulateApps(2);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     paged_apps_grid_view_);
  const gfx::Rect apps_grid_bounds = paged_apps_grid_view_->GetLocalBounds();
  gfx::Point to =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() + 1);

  // Drag down the first item until a new page is created.
  // For fullscreen, drag to the bottom/right of bounds.
  page_flip_waiter_->Reset();
  UpdateDrag(AppsGridView::MOUSE, to, paged_apps_grid_view_);
  while (HasPendingPageFlip(paged_apps_grid_view_))
    page_flip_waiter_->Wait();
  EndDrag(paged_apps_grid_view_, false /*cancel*/);

  ASSERT_EQ(GetPaginationModel()->total_pages(), 2);
  histogram_tester.ExpectBucketCount("Apps.AppList.AppsGridAddPage",
                                     AppListPageCreationType::kDraggingApp, 1);
}

TEST_F(AppsGridViewTest, CreateANewPageByAddingAppLogsMetrics) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(0));

  // Add an item to simulate installing or syncing, the metric should be
  // recorded.
  model_->CreateAndAddItem("Extra App");

  ASSERT_EQ(GetPaginationModel()->total_pages(), 2);
  histogram_tester.ExpectBucketCount("Apps.AppList.AppsGridAddPage",
                                     AppListPageCreationType::kSyncOrInstall,
                                     1);
}

// Test that the background cards remain stacked as the bottom layer during
// an item drag. The adding of views to the apps grid during a drag (e.g. ghost
// image view) can cause a reorder of layers.
TEST_P(AppsGridViewCardifiedStateTest, BackgroundCardLayerOrderedAtBottom) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  model_->PopulateApps(2);

  // Start cardified apps grid.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);
  ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(nullptr, GetCurrentGhostImageView());

  if (features::IsProductivityLauncherEnabled()) {
    test_api_->FireReorderTimerAndWaitForAnimationDone();
    // Check that the ghost image view was created.
    EXPECT_NE(nullptr, GetCurrentGhostImageView());
  } else {
    test_api_->LayoutToIdealBounds();
  }

  const int kExpectedBackgroundCardCount =
      features::IsProductivityLauncherEnabled() ? 1 : 2;
  ASSERT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  // Check that the first background card layer is stacked at the bottom.
  EXPECT_EQ(paged_apps_grid_view_->GetBackgroundCardLayerForTesting(0),
            GetItemsContainer()
                ->layer()
                ->children()[kExpectedBackgroundCardCount - 1]);
}

TEST_P(AppsGridViewCardifiedStateTest, PeekingCardOnLastPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  model_->PopulateApps(2);

  // Start cardified apps grid.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);

  EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());

  const int kExpectedBackgroundCardCount =
      features::IsProductivityLauncherEnabled() ? 1 : 2;
  EXPECT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
}

TEST_P(AppsGridViewCardifiedStateTest, BackgroundCardBounds) {
  ASSERT_TRUE(paged_apps_grid_view_);
  model_->PopulateApps(30);

  // Enter cardified state.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);
  ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());

  const int kExpectedBackgroundCardCount =
      features::IsProductivityLauncherEnabled() ? 2 : 3;
  ASSERT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  // Verify that all items in the current page fit within the background card.
  gfx::Rect background_card_bounds =
      paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(0);
  gfx::Rect clip_rect = paged_apps_grid_view_->GetMirroredRect(
      paged_apps_grid_view_->layer()->clip_rect());
  gfx::Rect first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

  EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << first_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << first_item_bounds.ToString();

  gfx::Rect last_item_bounds = GetItemRectOnCurrentPageAt(
      GetTilesPerPage(0) / apps_grid_view_->cols() - 1,
      apps_grid_view_->cols() - 1);

  EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << last_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << last_item_bounds.ToString();

  // Simulate screen rotation (r = 90 degrees clockwise).
  UpdateDisplay("1024x768/r");
  app_list_view_->OnParentWindowBoundsChanged();

  ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  ASSERT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  // Verify that all items in the current page fit within the background card.
  background_card_bounds =
      paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(0);
  clip_rect = paged_apps_grid_view_->GetMirroredRect(
      paged_apps_grid_view_->layer()->clip_rect());
  first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

  EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << first_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << first_item_bounds.ToString();

  last_item_bounds = GetItemRectOnCurrentPageAt(
      GetTilesPerPage(0) / apps_grid_view_->cols() - 1,
      apps_grid_view_->cols() - 1);

  EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << last_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << last_item_bounds.ToString();

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(gfx::Rect(), paged_apps_grid_view_->layer()->clip_rect());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(0, paged_apps_grid_view_->BackgroundCardCountForTesting());
}

TEST_P(AppsGridViewCardifiedStateTest, BackgroundCardBoundsOnSecondPage) {
  ASSERT_TRUE(paged_apps_grid_view_);
  model_->PopulateApps(30);

  // Enter cardified state, and drag the item to the second apps grid page.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     paged_apps_grid_view_);
  const gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).left_center();
  // Drag the first item to the next page to create another page.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);

  // Trigger cardified state again.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     paged_apps_grid_view_);

  ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  const int kExpectedBackgroundCardCount =
      features::IsProductivityLauncherEnabled() ? 2 : 3;
  ASSERT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  // Verify that all items in the current page fit within the background card.
  gfx::Rect background_card_bounds =
      paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(1);
  gfx::Rect clip_rect = paged_apps_grid_view_->GetMirroredRect(
      paged_apps_grid_view_->layer()->clip_rect());
  gfx::Rect first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

  EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << first_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << first_item_bounds.ToString();

  gfx::Rect last_item_bounds = GetItemRectOnCurrentPageAt(
      GetTilesPerPage(1) / apps_grid_view_->cols() - 1,
      apps_grid_view_->cols() - 1);

  EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << last_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << last_item_bounds.ToString();

  // Simulate screen rotation (r = 90 degrees clockwise).
  UpdateDisplay("1024x768/r");

  ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  ASSERT_EQ(kExpectedBackgroundCardCount,
            paged_apps_grid_view_->BackgroundCardCountForTesting());

  // Verify that all items in the current page fit within the background card.
  background_card_bounds =
      paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(1);
  clip_rect = paged_apps_grid_view_->GetMirroredRect(
      paged_apps_grid_view_->layer()->clip_rect());
  first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

  EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << first_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << first_item_bounds.ToString();

  last_item_bounds = GetItemRectOnCurrentPageAt(
      GetTilesPerPage(1) / apps_grid_view_->cols() - 1,
      apps_grid_view_->cols() - 1);

  EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
      << " background card bounds " << background_card_bounds.ToString()
      << " item bounds " << last_item_bounds.ToString();
  EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
      << " clip rect " << clip_rect.ToString() << " item bounds "
      << last_item_bounds.ToString();

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(gfx::Rect(), paged_apps_grid_view_->layer()->clip_rect());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(0, paged_apps_grid_view_->BackgroundCardCountForTesting());
}

// This is a NonBubble test because new empty pages cannot be created with the
// ProductivityLauncher feature.
TEST_P(AppsGridViewDragNonBubbleTest,
       PeekingCardOnLastPageAfterCreatingNewPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  model_->PopulateApps(2);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     paged_apps_grid_view_);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  const gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();

  // Drag the first item to the next page to create another page.
  UpdateDragToNeighborPage(true /* next_page */, to_in_next_page);
  // Trigger cardified state again.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::MOUSE, 0, 0,
                                     paged_apps_grid_view_);
  EXPECT_EQ(2, GetHapticTickEventsCount());

  EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(3, paged_apps_grid_view_->BackgroundCardCountForTesting());

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(2, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewCardifiedStateTest, AppsGridIsCardifiedDuringDrag) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  model_->PopulateApps(2);

  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     paged_apps_grid_view_);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
}

TEST_P(AppsGridViewCardifiedStateTest,
       DragWithinFolderDoesNotEnterCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Creates a folder item and open it.
  const size_t kTotalItems = kMaxItemsPerFolderPage;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());

  // Drag the first folder child within the folder.
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 0,
                                     folder_apps_grid_view());
  EXPECT_EQ(0, GetHapticTickEventsCount());
  const gfx::Point to =
      folder_grid_test_api.GetItemTileRectOnCurrentPageAt(0, 1).CenterPoint();
  UpdateDrag(AppsGridView::TOUCH, to, folder_apps_grid_view(), 10 /*steps*/);
  // The folder item reparent timer should not be triggered.
  ASSERT_FALSE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());

  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

TEST_P(AppsGridViewCardifiedStateTest, DragOutsideFolderEntersCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a folder item with some apps and open it.
  model_->CreateAndPopulateFolderWithApps(3);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view = InitiateDragForItemAtCurrentPageAt(
      AppsGridView::TOUCH, 0, 0, folder_apps_grid_view());
  EXPECT_EQ(0, GetHapticTickEventsCount());
  const gfx::Point to =
      app_list_folder_view()->GetLocalBounds().bottom_center() +
      gfx::Vector2d(0, drag_view->height()
                    /*padding to completely exit folder view*/);
  UpdateDrag(AppsGridView::TOUCH, to, folder_apps_grid_view(), 10 /*steps*/);
  // Fire the reparent timer that should be started when an item is dragged out
  // of folder bounds.
  ASSERT_TRUE(folder_apps_grid_view()->FireFolderItemReparentTimerForTest());

  EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());

  EndDrag(folder_apps_grid_view(), false /*cancel*/);
  EXPECT_EQ(0, GetHapticTickEventsCount());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
}

TEST_P(AppsGridViewCardifiedStateTest,
       DragItemIntoFolderStaysInCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a folder item with some apps. Add another app to the main grid.
  model_->CreateAndPopulateFolderWithApps(2);
  model_->PopulateApps(1);
  InitiateDragForItemAtCurrentPageAt(AppsGridView::TOUCH, 0, 1,
                                     paged_apps_grid_view_);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  // Dragging item_1 over folder to expand it.
  const gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  UpdateDrag(AppsGridView::TOUCH, to, paged_apps_grid_view_, 10 /*steps*/);

  EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());

  EndDrag(paged_apps_grid_view_, false /*cancel*/);
  EXPECT_EQ(0, GetHapticTickEventsCount());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewAppSortTest,
       ContextMenuInTopLevelAppListSortAllAppsInClamshellMode) {
  // In this test, the sort algorithm is not tested. Instead, the context menu
  // that contains the options to sort is verified to be shown in apps grid
  // view. The menu option selecting is also simulated to ensure the sorting is
  // called. The actual sort algorithm is tested in
  // chrome/browser/ui/app_list/app_list_sort_browsertest.cc.

  // The AppsGridContextMenu is only used in clamshell mode.
  if (create_as_tablet_mode_)
    return;

  model_->PopulateApps(1);

  AppsGridContextMenu* context_menu = apps_grid_view_->context_menu_for_test();
  EXPECT_FALSE(context_menu->IsMenuShowing());
  EXPECT_EQ(AppListSortOrder::kCustom, model_->requested_sort_order());

  // Get a point in `apps_grid_view_` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_grid_view_->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Cache the current context menu view.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(reorder_option->title() == u"Name");

  // Open the Reorder by Name submenu.
  const gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            model_->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Open the menu again to test the color sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_TRUE(reorder_option->title() == u"Color");

  const gfx::Point color_option =
      reorder_option->GetBoundsInScreen().CenterPoint();

  SimulateLeftClickOrTapAt(color_option);
  EXPECT_EQ(AppListSortOrder::kColor, model_->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());
}

TEST_P(AppsGridViewAppSortTest,
       ContextMenuInTopLevelAppListSortAllAppsInTabletMode) {
  // This test checks the context menu on root window in tablet mode.
  if (!create_as_tablet_mode_)
    return;

  model_->PopulateApps(1);
  EXPECT_EQ(AppListSortOrder::kCustom, model_->requested_sort_order());

  // Get a point in `apps_grid_view_` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_grid_view_->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  AppMenuModelAdapter* context_menu =
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing();
  EXPECT_TRUE(context_menu->IsShowingMenu());

  // Cache the current context menu view.
  views::MenuItemView* reorder_submenu =
      context_menu->root_for_testing()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_TRUE(reorder_submenu->title() == u"Sort by");

  // Open the Sort by submenu.
  gfx::Point reorder_submenu_point =
      reorder_submenu->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_submenu_point);

  views::MenuItemView* reorder_option =
      reorder_submenu->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(reorder_option->title() == u"Name");
  gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);

  // Check that the apps are sorted and the menu is closed.
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            model_->requested_sort_order());
  EXPECT_EQ(
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing(),
      nullptr);

  // Open the menu again to test the color sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  context_menu =
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing();
  EXPECT_TRUE(context_menu->IsShowingMenu());

  reorder_submenu =
      context_menu->root_for_testing()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_TRUE(reorder_submenu->title() == u"Sort by");

  // Open the Sort by submenu.
  reorder_submenu_point = reorder_submenu->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_submenu_point);

  reorder_option = reorder_submenu->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(reorder_option->title() == u"Color");
  reorder_option_point = reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);

  // Check that the apps are sorted and the menu is closed.
  EXPECT_EQ(AppListSortOrder::kColor, model_->requested_sort_order());
  EXPECT_EQ(
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing(),
      nullptr);
}

TEST_P(AppsGridViewAppSortTest, ContextMenuOnFolderItemSortAllApps) {
  // In this test, the sort algorithm is not tested. Instead, the context menu
  // that contains the options to sort is verified to be shown on folder app
  // list item view. The menu option selecting is also simulated to ensure the
  // sorting is called. The actual sort algorithm is tested in
  // chrome/browser/ui/app_list/app_list_sort_browsertest.cc.

  // Create a folder item and update the layout.
  model_->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();
  EXPECT_EQ(AppListSortOrder::kCustom, model_->requested_sort_order());

  // Get a point on the folder item.
  AppListItemView* folder_item = apps_grid_view_->view_model()->view_at(0);
  ASSERT_TRUE(folder_item->is_folder());
  gfx::Point folder_item_point = folder_item->GetBoundsInScreen().CenterPoint();

  AppsGridContextMenu* context_menu = folder_item->context_menu_for_folder();
  ASSERT_TRUE(context_menu);
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Open the menu to test the alphabetical sort option.
  SimulateRightClickOrLongPressAt(folder_item_point);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Cache the current context menu view.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(reorder_option->title() == u"Name");

  // Open the Reorder by Name submenu.
  gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            model_->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Open the menu again to test the color sort option.
  SimulateRightClickOrLongPressAt(folder_item_point);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_TRUE(reorder_option->title() == u"Color");

  const gfx::Point color_option =
      reorder_option->GetBoundsInScreen().CenterPoint();

  SimulateLeftClickOrTapAt(color_option);
  EXPECT_EQ(AppListSortOrder::kColor, model_->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());
}

TEST_F(AppsGridViewTest, PulsingBlocksShowDuringAppListSync) {
  model_->PopulateApps(3);
  UpdateLayout();
  EXPECT_EQ(0, GetPulsingBlocksModel().view_size());

  // Set the model status as syncing. The Pulsing blocks model should not be
  // empty.
  model_->SetStatus(AppListModelStatus::kStatusSyncing);
  UpdateLayout();
  EXPECT_NE(0, GetPulsingBlocksModel().view_size());

  // Set the model status as normal. The Pulsing blocks model should be empty.
  model_->SetStatus(AppListModelStatus::kStatusNormal);
  UpdateLayout();
  EXPECT_EQ(0, GetPulsingBlocksModel().view_size());
}

}  // namespace test
}  // namespace ash
