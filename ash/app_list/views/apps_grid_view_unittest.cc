// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/apps_grid_row_change_animator.h"
#include "ash/app_list/grid_index.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/quick_app_access_model.h"
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
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/drag_drop_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/system_shadow.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace test {

namespace {

constexpr size_t kMaxItemsInFolder = 48;

// The drag and drop app icon is scaled by this factor.
constexpr float kDragDropAppIconScale = 1.2f;

gfx::RectF GetViewBoundsWithCurrentTransform(views::View* view) {
  return view->layer()->transform().MapRect(
      gfx::RectF(view->GetMirroredBounds()));
}

std::optional<gfx::Vector2d> GetOffsetBetweenLayers(ui::Layer* source,
                                                    ui::Layer* target) {
  gfx::Vector2d offset;
  for (auto* current = source; current; current = current->parent()) {
    if (current == target) {
      return offset;
    }
    offset += current->bounds().OffsetFromOrigin();
  }
  return std::nullopt;
}

float CalculateManhattanDistance(gfx::Point p1, gfx::Point p2) {
  return std::abs(p1.x() - p2.x()) + std::abs(p1.y() - p2.y());
}

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;

  // ShelfModel::ShelfItemFactory:
  std::unique_ptr<ShelfItem> CreateShelfItemForApp(
      const ShelfID& shelf_id,
      ShelfItemStatus status,
      ShelfItemType shelf_item_type,
      const std::u16string& title) override {
    auto item = std::make_unique<ShelfItem>();
    item->id = shelf_id;
    item->status = status;
    item->type = shelf_item_type;
    item->title = title;
    return item;
  }

  std::unique_ptr<ShelfItemDelegate> CreateShelfItemDelegateForAppId(
      const std::string& app_id) override {
    return std::make_unique<TestShelfItemDelegate>(ShelfID(app_id));
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
  raw_ptr<PaginationModel> model_ = nullptr;
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
  raw_ptr<aura::Window, DanglingUntriaged> window_;
};

// Find the window with type WINDOW_TYPE_MENU and returns the firstly found one.
// Returns nullptr if no such window exists.
aura::Window* FindMenuWindow(aura::Window* root) {
  if (root->GetType() == aura::client::WINDOW_TYPE_MENU)
    return root;
  for (aura::Window* child : root->children()) {
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

  raw_ptr<PaginationModel> model_;
  base::OnceClosure task_;
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
  const raw_ptr<views::View> observed_view_;
  int bounds_change_count_ = 0;
};

// Records the longest scheduled animation duration for the given view.
class AnimationDurationRecorder {
 public:
  explicit AnimationDurationRecorder(AppListItemView* view) {
    view->EnsureLayer();
    subscription_ = view->layer()->GetAnimator()->AddSequenceScheduledCallback(
        base::BindRepeating(&AnimationDurationRecorder::OnSequenceScheduled,
                            base::Unretained(this)));
  }

  void OnSequenceScheduled(ui::LayerAnimationSequence* sequence) {
    // There can be more than one sequence scheduled for an animator, so keep
    // track of the largest animation duration.
    if (sequence->FirstElement()->duration() > largest_duration_) {
      largest_duration_ = sequence->FirstElement()->duration();
    }
  }

  base::TimeDelta largest_duration_;
  base::CallbackListSubscription subscription_;
};

}  // namespace

// Subclasses should set `is_rtl_`, `create_as_tablet_mode_`, etc. in their
// constructors to indicate which mode to test. By default, tests run in
// clamshell mode in English (left-to-right).
class AppsGridViewTest : public AshTestBase, views::WidgetObserver {
 public:
  AppsGridViewTest() = default;
  AppsGridViewTest(const AppsGridViewTest&) = delete;
  AppsGridViewTest& operator=(const AppsGridViewTest&) = delete;
  ~AppsGridViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kPromiseIcons, true}});
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    haptics_tracker_ = std::make_unique<HapticsTrackingTestInputController>();

    // Show the app list.
    auto* helper = GetAppListTestHelper();
    if (create_as_tablet_mode_) {
      // The app list will be shown automatically when tablet mode is enabled.
      ash::TabletModeControllerTestApi().EnterTabletMode();
    } else {
      helper->ShowAppList();
    }
    // Wait for any show animations to complete.
    base::RunLoop().RunUntilIdle();

    // Cache view pointers to make tests more concise.
    if (!create_as_tablet_mode_) {
      // AppsGridView is scrollable in clamshell mode.
      apps_grid_view_ = helper->GetScrollableAppsGridView();
      app_list_folder_view_ = helper->GetBubbleFolderView();
      search_box_view_ = helper->GetBubbleSearchBoxView();
    } else {
      app_list_view_ = helper->GetAppListView();
      app_list_folder_view_ = helper->GetFullscreenFolderView();
      auto* contents_view =
          app_list_view_->app_list_main_view()->contents_view();
      search_box_view_ = contents_view->GetSearchBoxView();
      // AppsGridView is paged in tablet mode.
      paged_apps_grid_view_ =
          contents_view->apps_container_view()->apps_grid_view();
      apps_grid_view_ = paged_apps_grid_view_;

      // In production, page flip duration > page transition > overscroll.
      SetPageFlipDurationForTest(paged_apps_grid_view_);
      page_flip_waiter_ =
          std::make_unique<PageFlipWaiter>(GetPaginationModel());
    }

    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view_);
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);

    // When root window where app list was last shown gets removed, the app list
    // view hierarchy gets reset (and rebuilt on next show). If a test removes a
    // display where app list is shown, all view pointers cached will become
    // invalid - add a app list widget observer to clean up the state if the app
    // list view gets removed before the test end.
    apps_grid_view_->GetWidget()->AddObserver(this);
  }

  void TearDown() override {
    if (apps_grid_view_)
      apps_grid_view_->GetWidget()->RemoveObserver(this);
    page_flip_waiter_.reset();
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    haptics_tracker_.reset();
    AshTestBase::TearDown();
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    apps_grid_view_ = nullptr;
    paged_apps_grid_view_ = nullptr;
    app_list_folder_view_ = nullptr;
    search_box_view_ = nullptr;
    test_api_.reset();
    page_flip_waiter_.reset();
  }

 protected:
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

  AppListItemList* GetTopLevelItemList() {
    return GetTestModel()->top_level_item_list();
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) {
    DCHECK_GT(GetTopLevelItemList()->item_count(), 0u);
    return test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  size_t GetTilesPerPageInPagedGrid(int page) const {
    return test_api_->TilesPerPageInPagedGrid(page);
  }

  size_t GetTilesPerPageOr(int page, size_t default_value) const {
    return test_api_->TilesPerPageOr(page, default_value);
  }

  PaginationModel* GetPaginationModel() const {
    DCHECK(paged_apps_grid_view_) << "Only available in tablet mode.";
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
    ui::KeyEvent key_event(ui::EventType::kKeyPressed, key_code, flags);
    apps_grid_view_->OnKeyPressed(key_event);
  }

  void SimulateKeyReleased(ui::KeyboardCode key_code, int flags) {
    ui::KeyEvent key_event(ui::EventType::kKeyReleased, key_code, flags);
    apps_grid_view_->OnKeyReleased(key_event);
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

  bool HasPendingPromiseAppRemoval(const std::string& promise_app_id) const {
    auto found =
        apps_grid_view_->pending_promise_apps_removals_.find(promise_app_id);

    return found != apps_grid_view_->pending_promise_apps_removals_.end();
  }

  // Simulates a long press on the point `location` if the test is in tablet
  // mode. Simulates a right click on the point otherwise. This function can be
  // used to open the context menu.
  void SimulateRightClickOrLongPressAt(const gfx::Point& location) {
    auto* event_generator = GetEventGenerator();
    if (create_as_tablet_mode_) {
      ui::GestureEvent gesture_event(
          location.x(), location.y(), 0, base::TimeTicks(),
          ui::GestureEventDetails(ui::EventType::kGestureLongPress));
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
    DCHECK_GT(view_model->view_size(), 0u);
    views::View* items_container = apps_grid_view_->items_container_;
    auto app_iter = items_container->FindChild(view_model->view_at(0));
    DCHECK(app_iter != items_container->children().cend());
    for (size_t i = 1; i < view_model->view_size(); ++i) {
      ++app_iter;
      ASSERT_NE(items_container->children().cend(), app_iter);
      EXPECT_EQ(view_model->view_at(i), *app_iter);
    }
  }

  views::View* GetItemsContainer() {
    return apps_grid_view_->items_container();
  }

  const views::ViewModelT<PulsingBlockView>& GetPulsingBlocksModel() const {
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
    if (!create_as_tablet_mode_)
      views::test::RunScheduledLayout(GetAppListTestHelper()->GetBubbleView());
    else
      views::test::RunScheduledLayout(app_list_view_);
  }

  AppListItemView* GetItemViewInCurrentPageAt(int row,
                                              int column,
                                              AppsGridView* apps_grid_view) {
    AppsGridViewTestApi test_api(apps_grid_view);
    const int selected_page = GetSelectedPage(apps_grid_view);
    GridIndex index(selected_page, row * apps_grid_view->cols() + column);
    return test_api.GetViewAtIndex(index);
  }

  AppListItemView* InitiateDragForItemAtCurrentPageAt(
      AppsGridView::Pointer pointer,
      int row,
      int column,
      AppsGridView* apps_grid_view) {
    AppListItemView* view =
        GetItemViewInCurrentPageAt(row, column, apps_grid_view);

    StartDragForViewAndFireTimer(pointer, view);
    TriggerDragFlow(pointer);
    return view;
  }

  void StartDragForViewAndFireTimer(AppsGridView::Pointer pointer,
                                    AppListItemView* view) {
    DCHECK(view);

    gfx::Point from = view->GetBoundsInScreen().CenterPoint();

    // TODO(anasalazar): Investigate icon jump by a few pixels when starting
    // drag.
    auto* generator = GetEventGenerator();
    if (pointer == AppsGridView::TOUCH) {
      generator->MoveTouch(from);
      generator->PressTouch();
      view->FireTouchDragTimerForTest();
    } else {
      generator->MoveMouseTo(from);
      generator->PressLeftButton();
      view->FireMouseDragTimerForTest();
    }
  }

  void TriggerDragFlow(AppsGridView::Pointer pointer) {
    // Call UpdateDrag to trigger |apps_grid_view| change to cardified_state -
    // the cardified state starts only once the drag distance exceeds a drag
    // threshold, so the pointer has to sufficiently move from the original
    // position.
    UpdateDragInScreen(
        pointer,
        GetEventGenerator()->current_screen_location() + gfx::Vector2d(10, 10),
        1);
    // A second smaller drag movement is needed to trigger OnDragEntered from
    // the DragDropController.
    UpdateDragInScreen(
        pointer,
        GetEventGenerator()->current_screen_location() + gfx::Vector2d(5, 5),
        1);
  }

  void UpdateDragInScreen(AppsGridView::Pointer pointer,
                          const gfx::Point& to_in_screen,
                          int steps = 1) {
    gfx::Point start(GetEventGenerator()->current_screen_location());
    for (int step = 1; step <= steps; step += 1) {
      gfx::Point drag_increment_point(start);
      drag_increment_point +=
          gfx::Vector2d((to_in_screen.x() - start.x()) * step / steps,
                        (to_in_screen.y() - start.y()) * step / steps);
      auto* generator = GetEventGenerator();
      if (pointer == AppsGridView::TOUCH) {
        generator->MoveTouch(drag_increment_point);
      } else {
        generator->MoveMouseTo(drag_increment_point);
      }
    }
  }

  // Updates the drag from the current drag location to the destination point
  // |to|. These coordinates are relative the |apps_grid_view| which may belong
  // to either the app list or an open folder view.
  void UpdateDrag(AppsGridView::Pointer pointer,
                  const gfx::Point& to,
                  AppsGridView* apps_grid_view,
                  int steps = 1) {
    gfx::Point to_in_screen(to);
    views::View::ConvertPointToScreen(apps_grid_view, &to_in_screen);

    UpdateDragInScreen(pointer, to_in_screen, steps);
  }

  void EndDrag(AppsGridView::Pointer pointer = AppsGridView::MOUSE) {
    auto* generator = GetEventGenerator();
    if (pointer == AppsGridView::TOUCH)
      generator->ReleaseTouch();
    else
      generator->ReleaseLeftButton();
  }

  // Simulate drag from the |from| point to either next or previous page's |to|
  // point.
  // Update drag to either next or previous page's |to| point.
  void UpdateDragToNeighborPage(bool next_page,
                                const gfx::Point& to,
                                AppsGridView::Pointer pointer) {
    ASSERT_TRUE(paged_apps_grid_view_) << "Only available in tablet mode.";
    const int selected_page = GetPaginationModel()->selected_page();
    DCHECK(selected_page >= 0 &&
           selected_page <= GetPaginationModel()->total_pages());

    // Calculate the point required to flip the page if an item is dragged to
    // it.
    const gfx::Rect apps_grid_bounds = paged_apps_grid_view_->GetLocalBounds();
    gfx::Point point_in_page_flip_buffer =
        gfx::Point(apps_grid_bounds.width() / 2,
                   next_page ? apps_grid_bounds.bottom() - 1 : 0);

    // Update dragging and relayout apps grid view after drag ends.
    PostPageFlipTask task(GetPaginationModel(),
                          base::BindLambdaForTesting([&]() {
                            UpdateDrag(pointer, to, paged_apps_grid_view_,
                                       /*steps=*/10);
                          }));
    page_flip_waiter_->Reset();
    UpdateDrag(pointer, point_in_page_flip_buffer, paged_apps_grid_view_,
               /*steps=*/10);
    while (HasPendingPageFlip(paged_apps_grid_view_)) {
      page_flip_waiter_->Wait();
    }
    EndDrag(pointer);
    test_api_->LayoutToIdealBounds();
  }

  ui::Layer* GetDragIconLayer(AppsGridView* apps_grid_view) {
    ui::Layer* drag_icon_layer = apps_grid_view->drag_image_layer_for_test();

    return drag_icon_layer;
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

  void CheckHaptickEventsCount(int number_events) {
    EXPECT_EQ(number_events, GetHapticTickEventsCount());
  }

  // Get the number of item layer copies used for the between row animation.
  int GetNumberOfRowChangeLayersForTest(AppsGridView* apps_grid_view) {
    return apps_grid_view->row_change_animator_
        ->GetNumberOfRowChangeLayersForTest();
  }

  views::View* GetNewInstallDot(AppListItemView* view) {
    return view->new_install_dot_;
  }

  bool IsUIStateDraggingForItemView(AppListItemView* item) {
    return item->ui_state_ == AppListItemView::UI_STATE_DRAGGING ||
           item->ui_state_ == AppListItemView::UI_STATE_TOUCH_DRAGGING;
  }

  AppListTestModel* GetTestModel() { return GetAppListTestHelper()->model(); }

  // May be a PagedAppsGridView in tablet mode or a ScrollableAppsGridView in
  // clamshell mode.
  raw_ptr<AppsGridView, DanglingUntriaged> apps_grid_view_ = nullptr;

  // May be owned by different parent views depending on tablet mode.
  raw_ptr<AppListFolderView, DanglingUntriaged> app_list_folder_view_ = nullptr;
  raw_ptr<SearchBoxView, DanglingUntriaged> search_box_view_ = nullptr;

  // These views exist in tablet mode.
  raw_ptr<PagedAppsGridView, DanglingUntriaged> paged_apps_grid_view_ = nullptr;
  raw_ptr<AppListView, DanglingUntriaged> app_list_view_ =
      nullptr;  // Owned by native widget.

  std::unique_ptr<AppsGridViewTestApi> test_api_;

  // True if the test screen is configured to work with RTL locale.
  bool is_rtl_ = false;
  // True if we set the test on tablet mode.
  bool create_as_tablet_mode_ = false;

  std::unique_ptr<PageFlipWaiter> page_flip_waiter_;

 private:
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  // Used to track haptics events sent during drag.
  std::unique_ptr<HapticsTrackingTestInputController> haptics_tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests suite to test both tablet and clamshell mode behavior.
class AppsGridViewClamshellAndTabletTest
    : public AppsGridViewTest,
      public testing::WithParamInterface<bool> {
 public:
  AppsGridViewClamshellAndTabletTest() { create_as_tablet_mode_ = GetParam(); }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewClamshellAndTabletTest,
                         testing::Bool());

class AppsGridViewDragTestBase : public AppsGridViewTest {
 public:
  AppsGridViewDragTestBase() = default;

  // AppsGridViewTest:
  void SetUp() override {
    AppsGridViewTest::SetUp();
    ShelfModel::Get()->SetShelfItemFactory(&shelf_item_factory_);
    auto* drag_drop_controller = ShellTestApi().drag_drop_controller();
    drag_drop_controller_test_api_ =
        std::make_unique<DragDropControllerTestApi>(drag_drop_controller);
    drag_drop_controller->SetDisableNestedLoopForTesting(true);
  }

  void TearDown() override {
    drag_drop_controller_test_api_.reset();
    ShelfModel::Get()->SetShelfItemFactory(nullptr);
    AppsGridViewTest::TearDown();
  }

  gfx::Point GetDragIconCenter() {
    views::Widget* widget = drag_drop_controller_test_api_->drag_image_widget();
    if (!widget) {
      return gfx::Point();
    }
    return widget->GetContentsView()->GetBoundsInScreen().CenterPoint();
  }

  bool IsDragIconAnimatingForGrid(AppsGridView* apps_grid_view) {
    ui::Layer* drag_icon_layer = GetDragIconLayer(apps_grid_view);

    if (!drag_icon_layer) {
      return false;
    }

    return drag_icon_layer->GetAnimator()->is_animating();
  }

 private:
  // Shelf item factory required for test that drag from apps grid to shelf.
  ShelfItemFactoryFake shelf_item_factory_;
  std::unique_ptr<DragDropControllerTestApi> drag_drop_controller_test_api_;
};

// Tests suite for app list items drag and drop tests. These tests are
// parameterized by RTL locale and drag and drop implementation.
class AppsGridViewDragTest : public AppsGridViewDragTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  AppsGridViewDragTest() { is_rtl_ = GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, AppsGridViewDragTest, testing::Bool());

class AppsGridViewFolderIconRefreshTest
    : public AppsGridViewDragTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AppsGridViewFolderIconRefreshTest() { is_rtl_ = GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(Rtl,
                         AppsGridViewFolderIconRefreshTest,
                         testing::Bool());

class AppsGridViewTabletDragTest : public AppsGridViewDragTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  AppsGridViewTabletDragTest() {
    is_rtl_ = GetParam();
    create_as_tablet_mode_ = true;
  }
};

INSTANTIATE_TEST_SUITE_P(Rtl, AppsGridViewTabletDragTest, testing::Bool());

// Test suite for clamshell mode, parameterized by RTL.
class AppsGridViewClamshellTest : public AppsGridViewTest,
                                  public testing::WithParamInterface<bool> {
 public:
  AppsGridViewClamshellTest() { is_rtl_ = GetParam(); }
};
INSTANTIATE_TEST_SUITE_P(All, AppsGridViewClamshellTest, testing::Bool());

// Test suite for verifying tablet mode apps grid behaviour, parameterized by
// RTL locale. Tablet mode uses "cardified" pages, where the apps grid shrinks
// to be smaller during dragging.
class AppsGridViewTabletTest : public AppsGridViewTest,
                               public testing::WithParamInterface<bool> {
 public:
  AppsGridViewTabletTest() {
    is_rtl_ = GetParam();
    create_as_tablet_mode_ = true;
  }
};
INSTANTIATE_TEST_SUITE_P(All, AppsGridViewTabletTest, testing::Bool());

// TODO(anasalazar): Consolidate with AppsGridViewTabletTest suite once drag and
// drop refactor code is cleaned up.
class AppsGridViewTabletTestWithDragAndDropRefactor
    : public AppsGridViewTabletTest {
 public:
  AppsGridViewTabletTestWithDragAndDropRefactor() {
    is_rtl_ = GetParam();
    create_as_tablet_mode_ = true;
  }
};
INSTANTIATE_TEST_SUITE_P(All,
                         AppsGridViewTabletTestWithDragAndDropRefactor,
                         testing::Bool());

// This does not test the font name or weight because ash_unittests returns
// different font lists than chrome (e.g. "DejaVu Sans" instead of "Roboto").
TEST_F(AppsGridViewTest, AppListItemViewFont) {
  GetTestModel()->PopulateApps(1);
  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  EXPECT_EQ(12, item_view->title()->font_list().GetFontSize());
}

// This does not test the font name or weight because ash_unittests returns
// different font lists than chrome (e.g. "DejaVu Sans" instead of "Roboto").
TEST_P(AppsGridViewTabletTest, AppListItemViewFont) {
  GetTestModel()->PopulateApps(1);
  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  EXPECT_EQ(13, item_view->title()->font_list().GetFontSize());
}

TEST_F(AppsGridViewTest, RemoveSelectedLastApp) {
  const int kTotalItems = 2;
  const int kLastItemIndex = kTotalItems - 1;

  GetTestModel()->PopulateApps(kTotalItems);

  AppListItemView* last_view = GetItemViewInTopLevelGrid(kLastItemIndex);
  apps_grid_view_->SetSelectedView(last_view);
  GetTestModel()->DeleteItem(GetTestModel()->GetItemName(kLastItemIndex));

  EXPECT_FALSE(apps_grid_view_->IsSelectedView(last_view));

  // No crash happens.
  AppListItemView* view = GetItemViewInTopLevelGrid(0);
  apps_grid_view_->SetSelectedView(view);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(view));
}

// Tests that the item list changed without user operations; this happens on
// active user switch. See https://crbug.com/980082.
TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseCrash) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  GetTestModel()->PopulateApps(cols * 2);
  UpdateLayout();

  AppListItemView* view0 = GetItemViewInTopLevelGrid(0);
  GetTopLevelItemList()->MoveItem(0, cols + 2);

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

TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseAnimation) {
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  GetTestModel()->PopulateApps(cols * 2);
  UpdateLayout();

  // NOTE: Dismissing the app list creates layers for item views as part of the
  // opacity animations, even in tests. https://crbug.com/1246567
  GetAppListTestHelper()->DismissAndRunLoop();
  ASSERT_FALSE(apps_grid_view_->GetWidget()->IsVisible());

  AppListItemView* view0 = GetItemViewInTopLevelGrid(0);
  GetTopLevelItemList()->MoveItem(0, cols + 2);

  // Make sure the logical location of the view.
  EXPECT_NE(view0, GetItemViewInTopLevelGrid(0));
  EXPECT_EQ(view0, GetItemViewInTopLevelGrid(cols + 2));

  // The item should be repositioned immediately when the widget is not visible.
  EXPECT_FALSE(apps_grid_view_->IsAnimatingView(view0));
  EXPECT_EQ(view0->bounds(), GetItemRectOnCurrentPageAt(1, 2));
}

// Test that when dragging an item between pages/rows, the apps grid items
// animate between rows correctly. Items should not animate vertically when the
// reorder placeholder is changed during drag.
TEST_P(AppsGridViewTabletTest, BetweenRowsAnimationOnDragToPreviousPage) {
  ASSERT_TRUE(paged_apps_grid_view_);
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) + 15);
  UpdateLayout();

  GetPaginationModel()->SelectPage(1 /*page*/, false /*animate*/);
  EXPECT_EQ(1, GetSelectedPage(paged_apps_grid_view_));
  EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest(apps_grid_view_));

  // Begin dragging the third item of the second page.
  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 2, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the current item to flip to the first page.
    gfx::Point point_in_page_flip_buffer =
        gfx::Point(paged_apps_grid_view_->bounds().width() / 2, 0);
    UpdateDrag(AppsGridView::MOUSE, point_in_page_flip_buffer,
               paged_apps_grid_view_, 10 /*steps*/);
    while (HasPendingPageFlip(paged_apps_grid_view_)) {
      page_flip_waiter_->Wait();
    }
    EXPECT_EQ(0, GetSelectedPage(paged_apps_grid_view_));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move dragged item to the second slot on the first page.
    gfx::Point to;
    if (is_rtl_) {
      to = GetItemRectOnCurrentPageAt(0, 0).left_center();
    } else {
      to = GetItemRectOnCurrentPageAt(0, 0).right_center();
    }
    UpdateDrag(AppsGridView::MOUSE, to, paged_apps_grid_view_, 5 /*steps*/);

    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Testing animations require non-zero duration.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();

    const views::ViewModelT<AppListItemView>* view_model =
        apps_grid_view_->view_model();

    // The reorder placeholder should be after the first item. This will cause
    // the following items to animate one slot over, overflowing to the second
    // page.
    EXPECT_EQ(GridIndex(0, 1), paged_apps_grid_view_->reorder_placeholder());

    // Four items should have a layer copy used for animating between rows.
    EXPECT_EQ(4, GetNumberOfRowChangeLayersForTest(apps_grid_view_));

    for (size_t i = 1; i < view_model->view_size(); i++) {
      AppListItemView* item_view = view_model->view_at(i);
      // The first item and items off screen on the second page should not
      // animate.
      if (i == 0 || i > GetTilesPerPageInPagedGrid(0) + 1) {
        EXPECT_FALSE(apps_grid_view_->IsAnimatingView(item_view));
        continue;
      }

      // Check that none of the items are animating vertically, because any
      // items moving vertically should instead use a between rows animation,
      // which is purely horizontal.
      EXPECT_TRUE(apps_grid_view_->IsAnimatingView(item_view));
      gfx::Rect current_bounds_in_animation =
          gfx::ToRoundedRect(GetViewBoundsWithCurrentTransform(item_view));
      EXPECT_EQ(current_bounds_in_animation.y(), item_view->bounds().y());
      EXPECT_NE(current_bounds_in_animation.x(), item_view->bounds().x());
    }
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End the drag and check that no more item layer copies remain.
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->WaitForItemMoveAnimationDone();
  EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest(apps_grid_view_));
}

// Test that, within a 4 item folder, a row change animation only triggers when
// moving an item between rows and not when moving from one side of the grid to
// the other side in the same row.
TEST_P(AppsGridViewClamshellAndTabletTest, InFolderBetweenRowsAnimation) {
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(4);

  // Open the folder
  test_api_->PressItemAt(0);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(4u, folder_item->ChildItemCount());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  AppListItemView* const dragged_item_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item over slot 1.
    UpdateDrag(AppsGridView::MOUSE,
               folder_apps_grid_view()
                   ->GetItemViewAt(1)
                   ->GetBoundsInScreen()
                   .CenterPoint(),
               folder_apps_grid_view(), 5 /*steps*/);

    ASSERT_TRUE(folder_apps_grid_view()->reorder_timer_for_test()->IsRunning());
    folder_apps_grid_view()->reorder_timer_for_test()->FireNow();
    // Check that there is no row change animation for a drag from slot 0 to
    // slot 1.
    EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest(folder_apps_grid_view()));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item over slot 2.
    UpdateDrag(AppsGridView::MOUSE,
               folder_apps_grid_view()
                   ->GetItemViewAt(2)
                   ->GetBoundsInScreen()
                   .CenterPoint(),
               folder_apps_grid_view(), 5 /*steps*/);

    ASSERT_TRUE(folder_apps_grid_view()->reorder_timer_for_test()->IsRunning());
    folder_apps_grid_view()->reorder_timer_for_test()->FireNow();

    // Check that there is a row change animation for a drag from slot 1 to
    // slot 2.
    EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest(folder_apps_grid_view()));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
}

// Test dragging an app item from the first row to second row, and then back to
// the first row. This causes the between rows animation to reverse.
TEST_P(AppsGridViewTabletTest, BetweenRowsAnimationReversal) {
  ASSERT_TRUE(paged_apps_grid_view_);
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0));
  UpdateLayout();

  // Use non-zero animations to test that animations are correct while in
  // progress.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest(apps_grid_view_));

  // Begin dragging the first item.
  AppListItemView* const dragged_item = GetItemViewInTopLevelGrid(0);

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_item);

  // Wait for cardified animations to complete before testing row change
  // animations.
  test_api_->WaitForItemMoveAnimationDone();

  gfx::Point to;
  int first_row_y;
  int second_row_y;
  AppListItemView* item_view;
  gfx::Rect target_bounds;
  gfx::RectF current_bounds_in_animation;

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move dragged item to the middle slot on the second row.
    if (is_rtl_) {
      to = GetItemRectOnCurrentPageAt(0, 7).left_center();
    } else {
      to = GetItemRectOnCurrentPageAt(0, 7).right_center();
    }
    UpdateDrag(AppsGridView::MOUSE, to, paged_apps_grid_view_, 5 /*steps*/);

    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();

    // The reorder placeholder should be on the second row.
    EXPECT_EQ(GridIndex(0, 7), paged_apps_grid_view_->reorder_placeholder());

    const views::ViewModelT<AppListItemView>* view_model =
        apps_grid_view_->view_model();

    // View at index 0, 5 should be animating from second row to the first.
    item_view = view_model->view_at(5);

    first_row_y = GetItemRectOnCurrentPageAt(0, 0).y();
    second_row_y = GetItemRectOnCurrentPageAt(0, 6).y();
    EXPECT_GT(second_row_y, first_row_y);
    EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest(apps_grid_view_));

    // The item in slot 5 should now be on animating into the first row
    // position.
    EXPECT_EQ(item_view->bounds().y(), first_row_y);
    EXPECT_TRUE(apps_grid_view_->IsAnimatingView(item_view));
    target_bounds = GetItemRectOnCurrentPageAt(0, 4);
    current_bounds_in_animation = GetViewBoundsWithCurrentTransform(item_view);

    if (is_rtl_) {
      EXPECT_LT(current_bounds_in_animation.x(), target_bounds.x());
    } else {
      EXPECT_GT(current_bounds_in_animation.x(), target_bounds.x());
    }
    EXPECT_EQ(current_bounds_in_animation.y(), first_row_y);

    EXPECT_FALSE(item_view->GetTransform().IsIdentity());
    test_api_->WaitForItemMoveAnimationDone();
    current_bounds_in_animation = GetViewBoundsWithCurrentTransform(item_view);

    EXPECT_TRUE(item_view->GetTransform().IsIdentity());
    EXPECT_EQ(target_bounds, item_view->GetMirroredBounds());
    EXPECT_EQ(gfx::ToRoundedRect(current_bounds_in_animation), target_bounds);

    EXPECT_EQ(current_bounds_in_animation.y(), first_row_y);
    EXPECT_EQ(target_bounds.y(), first_row_y);
    EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest(apps_grid_view_));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Update drag to move placeholder back to the first row.
    if (is_rtl_) {
      to = GetItemRectOnCurrentPageAt(0, 0).left_center();
    } else {
      to = GetItemRectOnCurrentPageAt(0, 0).right_center();
    }
    // Move the drag to the first row, causing `item_view` to animate into
    // the second row.
    UpdateDrag(AppsGridView::MOUSE, to, paged_apps_grid_view_, 5 /*steps*/);

    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();

    // The reorder placeholder should now be on the first row.
    EXPECT_EQ(GridIndex(0, 1), paged_apps_grid_view_->reorder_placeholder());

    // The item in slot 5 should now be animating from first row to the second.
    EXPECT_EQ(item_view->GetMirroredBounds().y(), second_row_y);
    EXPECT_TRUE(apps_grid_view_->IsAnimatingView(item_view));

    // Item should be moving from offscreen into target position on second row.
    target_bounds = GetItemRectOnCurrentPageAt(0, 5);
    current_bounds_in_animation = GetViewBoundsWithCurrentTransform(item_view);

    if (is_rtl_) {
      EXPECT_GT(current_bounds_in_animation.x(), target_bounds.x());
    } else {
      EXPECT_LT(current_bounds_in_animation.x(), target_bounds.x());
    }
    EXPECT_EQ(current_bounds_in_animation.y(), second_row_y);

    test_api_->WaitForItemMoveAnimationDone();
    current_bounds_in_animation = GetViewBoundsWithCurrentTransform(item_view);

    EXPECT_TRUE(item_view->GetTransform().IsIdentity());
    EXPECT_EQ(target_bounds, item_view->GetMirroredBounds());
    EXPECT_EQ(gfx::ToRoundedRect(current_bounds_in_animation), target_bounds);

    EXPECT_EQ(current_bounds_in_animation.y(), second_row_y);
    EXPECT_EQ(target_bounds.y(), second_row_y);
    EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest(apps_grid_view_));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End the drag and check that no more item layer copies remain.
    EndDrag(AppsGridView::MOUSE);
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->WaitForItemMoveAnimationDone();
  EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest(apps_grid_view_));
}

// Test that cascading item animation durations are correct when an item moves
// from a top row to a bottom row.
TEST_P(AppsGridViewClamshellTest, CascadingItemAnimationMoveItemTopToBottom) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetTestModel()->PopulateApps(20);
  UpdateLayout();

  // The expected animation duration for items on each row. Each subsequent row
  // should have a duration that is 50ms longer than the last.
  base::TimeDelta first_row_duration = base::Milliseconds(300);
  base::TimeDelta second_row_duration = base::Milliseconds(350);
  base::TimeDelta third_row_duration = base::Milliseconds(400);
  base::TimeDelta fourth_row_duration = base::Milliseconds(450);

  std::vector<base::TimeDelta> expected_durations;
  expected_durations.insert(expected_durations.end(), 5, first_row_duration);
  expected_durations.insert(expected_durations.end(), 5, second_row_duration);
  expected_durations.insert(expected_durations.end(), 5, third_row_duration);
  expected_durations.insert(expected_durations.end(), 4, fourth_row_duration);

  std::vector<std::unique_ptr<AnimationDurationRecorder>> actual_durations;

  // Create a duration recorder for all item views starting at the second item,
  // since the very first item is the one being moved.
  for (size_t i = 1; i < GetTopLevelItemList()->item_count(); ++i) {
    AppListItemView* view = GetItemViewInTopLevelGrid(i);

    // Create an AnimationDurationRecorder to record the animation duration
    // for each item view's layer animation.
    actual_durations.push_back(
        std::make_unique<AnimationDurationRecorder>(view));
  }

  // Set hidden the item to be moved in the apps grid, so the item is ignored
  // in cascading animation setup.
  apps_grid_view_->set_hidden_view_for_test(GetItemViewInTopLevelGrid(0));

  // Move the first item to the last slot, causing a cascading item animation.
  GetTopLevelItemList()->MoveItem(0, 19);

  // Check that the expected duration of each item animation is correct.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    EXPECT_EQ(expected_durations[i], actual_durations[i]->largest_duration_);
  }
}

// Test that cascading item animation durations are correct when an item moves
// from a bottom row to a top row.
TEST_P(AppsGridViewClamshellTest, CascadingItemAnimationMoveItemBottomToTop) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetTestModel()->PopulateApps(20);
  UpdateLayout();

  // The expected animation duration for items on each row. Each subsequent row
  // should have a duration that is 50ms shorter than the last.
  base::TimeDelta first_row_duration = base::Milliseconds(450);
  base::TimeDelta second_row_duration = base::Milliseconds(400);
  base::TimeDelta third_row_duration = base::Milliseconds(350);
  base::TimeDelta fourth_row_duration = base::Milliseconds(300);

  std::vector<base::TimeDelta> expected_durations;
  expected_durations.insert(expected_durations.end(), 4, first_row_duration);
  expected_durations.insert(expected_durations.end(), 5, second_row_duration);
  expected_durations.insert(expected_durations.end(), 5, third_row_duration);
  expected_durations.insert(expected_durations.end(), 5, fourth_row_duration);

  std::vector<std::unique_ptr<AnimationDurationRecorder>> actual_durations;

  // Create a duration recorder for all item views except the last item, since
  // the last item is the one being moved.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    AppListItemView* view = GetItemViewInTopLevelGrid(i);

    // Create an AnimationDurationRecorder to record the animation duration
    // for each item view's layer animation.
    actual_durations.push_back(
        std::make_unique<AnimationDurationRecorder>(view));
  }

  // Set hidden the item to be moved in the apps grid, so the item is ignored
  // in cascading animation setup.
  apps_grid_view_->set_hidden_view_for_test(GetItemViewInTopLevelGrid(19));

  // Move the last item to the first slot, causing a cascading item animation.
  GetTopLevelItemList()->MoveItem(19, 0);

  // Check that the expected duration of each item animation is correct.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    EXPECT_EQ(expected_durations[i], actual_durations[i]->largest_duration_);
  }
}

// Test that cascading item animation durations are correct when an item moves
// within a single row, from the left side to the right side.
TEST_P(AppsGridViewClamshellTest, CascadingItemAnimationMoveItemLeftToRight) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetTestModel()->PopulateApps(5);
  UpdateLayout();

  // The expected animation duration for items in each slot. Each subsequent
  // item should have a duration that is 50ms longer than the last.
  std::vector<base::TimeDelta> expected_durations;
  expected_durations.push_back(base::Milliseconds(300));
  expected_durations.push_back(base::Milliseconds(350));
  expected_durations.push_back(base::Milliseconds(400));
  expected_durations.push_back(base::Milliseconds(450));

  std::vector<std::unique_ptr<AnimationDurationRecorder>> actual_durations;

  // Create a duration recorder for all item views except the first, since the
  // first item is the one being moved.
  for (size_t i = 1; i < GetTopLevelItemList()->item_count(); ++i) {
    AppListItemView* view = GetItemViewInTopLevelGrid(i);

    // Create an AnimationDurationRecorder to record the animation duration
    // for each item view's layer animation.
    actual_durations.push_back(
        std::make_unique<AnimationDurationRecorder>(view));
  }

  // Set hidden the item to be moved in the apps grid, so the item is ignored
  // in cascading animation setup.
  apps_grid_view_->set_hidden_view_for_test(GetItemViewInTopLevelGrid(0));

  // Move the first item to the row to the last slot in the row, causing a
  // cascading item animation.
  GetTopLevelItemList()->MoveItem(0, 4);

  // Check that the expected duration of each item animation is correct.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    EXPECT_EQ(expected_durations[i], actual_durations[i]->largest_duration_);
  }
}

// Test that cascading item animation durations are correct when an item moves
// within a single row, from the right side to the left side.
TEST_P(AppsGridViewClamshellTest, CascadingItemAnimationMoveItemRightToLeft) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetTestModel()->PopulateApps(5);
  UpdateLayout();

  // The expected animation duration for items in each slot. Each subsequent
  // item should have a duration that is 50ms shorter than the last.
  std::vector<base::TimeDelta> expected_durations;
  expected_durations.push_back(base::Milliseconds(450));
  expected_durations.push_back(base::Milliseconds(400));
  expected_durations.push_back(base::Milliseconds(350));
  expected_durations.push_back(base::Milliseconds(300));

  std::vector<std::unique_ptr<AnimationDurationRecorder>> actual_durations;

  // Create a duration recorder for all item views except the last item,
  // since the last item is the one being moved.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    AppListItemView* view = GetItemViewInTopLevelGrid(i);

    // Create an AnimationDurationRecorder to record the animation duration
    // for each item view's layer animation.
    actual_durations.push_back(
        std::make_unique<AnimationDurationRecorder>(view));
  }

  // Set hidden the item to be moved in the apps grid, so the item is ignored
  // in cascading animation setup.
  apps_grid_view_->set_hidden_view_for_test(GetItemViewInTopLevelGrid(4));

  // Move the last item in the row to the first slot in the row, causing a
  // cascading item animation.
  GetTopLevelItemList()->MoveItem(4, 0);

  // Check that the expected duration of each item animation is correct.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count() - 1; ++i) {
    EXPECT_EQ(expected_durations[i], actual_durations[i]->largest_duration_);
  }
}

TEST_F(AppsGridViewTest, ItemTooltip) {
  std::string title("a");
  AppListItem* item = GetTestModel()->CreateAndAddItem(title);
  GetTestModel()->SetItemName(item, title);

  AppListItemView* item_view = GetItemViewInTopLevelGrid(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_TRUE(
      title_label->GetTooltipText(title_label->bounds().CenterPoint()).empty());
  EXPECT_EQ(base::ASCIIToUTF16(title), title_label->GetText());
}

TEST_P(AppsGridViewTabletTest,
       OnGestureEventScrollSequenceHandleByPaginationController) {
  base::HistogramTester histogram_tester;

  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) + 1);
  UpdateLayout();
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, -1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, -10));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  // Drag up on the app grid when on page 1, this should move the AppsGridView
  // but not the AppListView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 0);

  apps_grid_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_NE(0, GetPaginationModel()->transition().progress);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "TabletMode",
      0);

  apps_grid_view_->OnGestureEvent(&scroll_end);

  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
      "TabletMode",
      1);
}

// Tests that taps between apps within the AppsGridView does not result in the
// AppList closing.
TEST_F(AppsGridViewTest, TapsBetweenAppsWontCloseAppList) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  // Simulate a tap between the two apps.
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  ui::GestureEvent gesture_event(
      between_apps.x(), between_apps.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  apps_grid_view_->OnGestureEvent(&gesture_event);

  // App list is still visible.
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
}

TEST_F(AppsGridViewTest, FolderColsAndRows) {
  // Populate folders with different number of apps.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  GetTestModel()->CreateAndPopulateFolderWithApps(9);
  GetTestModel()->CreateAndPopulateFolderWithApps(15);
  GetTestModel()->CreateAndPopulateFolderWithApps(17);

  // Check the number of cols and rows for each opened folder.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  AppsGridViewTestApi folder_grid_test_api(items_grid_view);
  test_api_->PressItemAt(0);
  EXPECT_EQ(2u, items_grid_view->view_model()->view_size());
  EXPECT_EQ(2, items_grid_view->cols());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(1);
  EXPECT_EQ(5u, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(2);
  EXPECT_EQ(9u, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(3);
  EXPECT_EQ(15u, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(4);
  EXPECT_EQ(17u, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  app_list_folder_view()->CloseFolderPage();
}

TEST_F(AppsGridViewTest, RemoveItemsInFolderShouldUpdateBounds) {
  // Populate two folders with different number of apps.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  AppListFolderItem* folder_2 =
      GetTestModel()->CreateAndPopulateFolderWithApps(4);

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
  GetTestModel()->DeleteItem(folder_2->item_list()->item_at(0)->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            two_rows_folder_view.size());

  // Remove another item from the folder. The bound should update and become the
  // folder view with one row.
  GetTestModel()->DeleteItem(folder_2->item_list()->item_at(0)->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            one_row_folder_view.size());
}

TEST_P(AppsGridViewClamshellAndTabletTest, AddItemsToFolderShouldUpdateBounds) {
  // Populate two folders with different number of apps.
  AppListFolderItem* folder_1 =
      GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->CreateAndPopulateFolderWithApps(4);

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
  GetTestModel()->AddItemToFolder(GetTestModel()->CreateItem("Extra 1"),
                                  folder_1->id());
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  items_grid_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            two_rows_folder_view.size());
  app_list_folder_view()->CloseFolderPage();

  // Create and add an almost full folder. Add an item to the folder should
  // not change the size of the folder view.
  AppListFolderItem* folder_full =
      GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder - 1);
  test_api_->PressItemAt(2);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect full_folder_view = items_grid_view->GetBoundsInScreen();

  GetTestModel()->AddItemToFolder(GetTestModel()->CreateItem("Extra 2"),
                                  folder_full->id());
  EXPECT_EQ(items_grid_view->GetBoundsInScreen().size(),
            full_folder_view.size());
  app_list_folder_view()->CloseFolderPage();
}

TEST_P(AppsGridViewTabletTest, ScrollDownShouldNotExitFolder) {
  const size_t kTotalItems = kMaxItemsInFolder;
  GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            GetTopLevelItemList()->item_at(0)->GetItemType());

  // Open the folder.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Vertically scroll the folder's scroll view.
  views::ScrollView* scroll_view =
      app_list_folder_view()->scroll_view_for_test();
  scroll_view->vertical_scroll_bar()->ScrollByContentsOffset(10);

  // Folder is still open.
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
}

// Tests that an app icon is selected when a menu is shown by click.
TEST_F(AppsGridViewTest, AppIconSelectedWhenMenuIsShown) {
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  ASSERT_EQ(1u, GetTopLevelItemList()->item_count());
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
TEST_P(AppsGridViewTabletTest, MenuAtRightPosition) {
  const size_t kItemsInPage = GetTilesPerPageInPagedGrid(0);
  const size_t kPages = 2;
  GetTestModel()->PopulateApps(kItemsInPage * kPages);
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
      ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(),
                                 ui::EF_RIGHT_MOUSE_BUTTON,
                                 ui::EF_RIGHT_MOUSE_BUTTON);
      static_cast<views::View*>(item_view)->OnMouseEvent(&press_event);

      ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
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

TEST_F(AppsGridViewTest, ItemViewsDontHaveLayer) {
  size_t kTotalItems = 3;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();

  // Normally individual item-view does not have a layer.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count(); ++i) {
    EXPECT_FALSE(GetItemViewInTopLevelGrid(i)->layer());
  }
}

TEST_P(AppsGridViewDragTest, DismissWhileDraggingDoesNotCrash) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  // Non-zero animation durations are necessary to make sure we don't miss
  // crashes involving animation delegates. Specifically, `bounds_animator_` had
  // a use after free problem in the past.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());

    GetAppListTestHelper()->Dismiss();
    CheckHaptickEventsCount(1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // No crash
}

TEST_P(AppsGridViewDragTest, DraggingTypeMouse) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::NONE);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::MOUSE);
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::MOUSE); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  CheckHaptickEventsCount(1);
  EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::NONE);
}

TEST_P(AppsGridViewDragTest, DraggingTypeTouch) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::TOUCH, item_view);

  EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::NONE);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::TOUCH);
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/true);

  CheckHaptickEventsCount(0);
  EXPECT_EQ(apps_grid_view_->drag_pointer(), AppsGridView::NONE);
}

TEST_P(AppsGridViewDragTest, DismissWhileDraggingInFolderDoesNotCrash) {
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  test_api_->Update();
  test_api_->PressItemAt(0);

  // Non-zero animation durations are necessary to make sure we don't miss
  // crashes involving animation delegates. Specifically, `bounds_animator_` had
  // a use after free problem in the past.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    ASSERT_TRUE(folder_apps_grid_view()->drag_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
    ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

    GetAppListTestHelper()->Dismiss();
    CheckHaptickEventsCount(1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);
  // No crash
}

TEST_P(AppsGridViewDragTest, ItemViewsHaveLayerDuringDrag) {
  size_t kTotalItems = 3;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_1 over item_0 creates a folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Each item view has its own layer during the drag.
    for (size_t i = 0; i < GetTopLevelItemList()->item_count(); ++i) {
      EXPECT_TRUE(GetItemViewInTopLevelGrid(i)->layer());
    }
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, ItemViewsDontHaveLayerAfterDrag) {
  size_t kTotalItems = 3;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_1 over item_0 creates a folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->WaitForItemMoveAnimationDone();
  CheckHaptickEventsCount(1);

  // The layer should be destroyed after the dragging.
  for (size_t i = 0; i < GetTopLevelItemList()->item_count(); ++i) {
    EXPECT_FALSE(GetItemViewInTopLevelGrid(i)->layer());
  }
}

TEST_P(AppsGridViewFolderIconRefreshTest, AppIconExtendState) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  size_t kTotalItems = 2;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));
  gfx::Point from;
  gfx::Point to;
  AppListItemView* extended_app;
  const ui::Layer* icon_background_layer;

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Drag item_1 over item_0.
    from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
    extended_app = GetItemViewInAppsGridAt(0, apps_grid_view_);

    EXPECT_TRUE(extended_app->is_icon_extended_for_test());
    icon_background_layer = extended_app->icon_background_layer_for_test();
    EXPECT_TRUE(icon_background_layer);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Quickly move the dragged app out and back to item_0. Make sure the
    // background layer is not recreated.
    UpdateDrag(AppsGridView::MOUSE, from, apps_grid_view_, 1 /*steps*/);
    EXPECT_FALSE(extended_app->is_icon_extended_for_test());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 1 /*steps*/);
    EXPECT_TRUE(extended_app->is_icon_extended_for_test());
    EXPECT_EQ(icon_background_layer,
              extended_app->icon_background_layer_for_test());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the dragged app to its original position and check if the background
    // layer still exists.
    UpdateDrag(AppsGridView::MOUSE, from, apps_grid_view_, 1 /*steps*/);
    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(const_cast<ui::Layer*>(icon_background_layer));
    EXPECT_FALSE(extended_app->is_icon_extended_for_test());
    ASSERT_TRUE(extended_app->icon_background_layer_for_test());
    EXPECT_FALSE(extended_app->icon_background_layer_for_test()->IsVisible());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

TEST_P(AppsGridViewFolderIconRefreshTest, FolderIconExtendState) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a folder and an app.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  AppListItemView* folder_view = GetItemViewInAppsGridAt(0, apps_grid_view_);
  auto* background_layer = folder_view->icon_background_layer_for_test();

  // The icon_background_layer is only created if the icon refresh is enabled.
  EXPECT_TRUE(background_layer);

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));
  gfx::Point from;
  gfx::Point to;

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Drag the app over the folder.
    from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
    EXPECT_TRUE(folder_view->is_icon_extended_for_test());
    EXPECT_EQ(background_layer, folder_view->icon_background_layer_for_test());
    EXPECT_TRUE(background_layer->IsVisible());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Quickly move the dragged app out and back to item_0. Make sure the
    // background layer is not recreated.
    UpdateDrag(AppsGridView::MOUSE, from, apps_grid_view_, 1 /*steps*/);
    EXPECT_FALSE(folder_view->is_icon_extended_for_test());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 1 /*steps*/);
    EXPECT_TRUE(folder_view->is_icon_extended_for_test());
    EXPECT_EQ(background_layer, folder_view->icon_background_layer_for_test());
    EXPECT_TRUE(background_layer->IsVisible());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Release the drag.
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(const_cast<ui::Layer*>(background_layer));
  EXPECT_FALSE(folder_view->is_icon_extended_for_test());
  EXPECT_EQ(background_layer, folder_view->icon_background_layer_for_test());
  EXPECT_TRUE(background_layer->IsVisible());
}

TEST_P(AppsGridViewFolderIconRefreshTest, FolderIconItemCounter) {
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->CreateAndPopulateFolderWithApps(4);
  GetTestModel()->CreateAndPopulateFolderWithApps(10);
  GetTestModel()->CreateAndPopulateFolderWithApps(104);

  // The number that will be shown on the folder icon follows the following
  // rules:
  // 1. The icon counter will not be showing if the number of items in a folder
  // is less or equal to 4, where all items can be painted on the icon.
  // 2. The counter shows how many items are not drawn on the icon, which is
  // (the number of items - 3).
  // 3. The maximum number that can be shown is 100.
  std::vector<std::optional<size_t>> expected_counts = {std::nullopt,
                                                        std::nullopt, 7, 100};
  UpdateLayout();

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(GetItemViewInTopLevelGrid(i)->item_counter_count_for_test(),
              expected_counts[i]);
  }
}

TEST_P(AppsGridViewDragTest, MouseDragItemIntoFolder) {
  size_t kTotalItems = 3;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_1 over item_0 creates a folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();

  EXPECT_EQ(kTotalItems - 1, GetTopLevelItemList()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            GetTopLevelItemList()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(GetTopLevelItemList()->item_at(0));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  AppListItem* item_0 = GetTestModel()->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  AppListItem* item_1 = GetTestModel()->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  std::string expected_items = folder_item->id() + ",Item 2";
  EXPECT_EQ(expected_items, GetTestModel()->GetModelContent());

  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
  EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                  ->GetFolderNameViewForTest()
                  ->HasFocus());
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, MouseDragSecondItemIntoFolder) {
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_2 to the folder adds Item_2 to the folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();

  EXPECT_EQ(1u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(folder_item->id(), GetTestModel()->GetModelContent());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  AppListItem* item_0 = GetTestModel()->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  AppListItem* item_1 = GetTestModel()->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  AppListItem* item_2 = GetTestModel()->FindItem("Item 2");
  EXPECT_TRUE(item_2->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_2->folder_id());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToFolder) {
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(1);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_2 to the folder adds Item_2 to the folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  ASSERT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  ui::Layer* drag_icon_layer = GetDragIconLayer(apps_grid_view_);
  ASSERT_TRUE(drag_icon_layer);
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToCreateFolder) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Dragging item_1 over item_0 creates a folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  ASSERT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  ui::Layer* drag_icon_layer = GetDragIconLayer(apps_grid_view_);
  ASSERT_TRUE(drag_icon_layer);
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesToTargetItemBounds) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  // Start drag from centerpoint of item_view
  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    const gfx::Point drop_point =
        GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, drop_point, apps_grid_view_, 5 /*steps*/);
  }));

  // End drag, and verify target drop icon bounds.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enable drop animation, as the test is verifying target animated
    // transform/bounds.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    EndDrag();

    gfx::Rect final_item_icon_bounds = item_view->GetIconBounds();
    views::View::ConvertRectToScreen(item_view, &final_item_icon_bounds);

    ui::Layer* const drag_icon_layer = GetDragIconLayer(apps_grid_view_);
    // Get drag icon layer's target position relative to the layer target
    // bounds.
    gfx::Rect drag_icon_target_bounds =
        drag_icon_layer->GetTargetTransform().MapRect(
            gfx::Rect(drag_icon_layer->GetTargetBounds().size()));

    // Convert the drag icon target bounds to the layer of the root window that
    // host the drag icon.
    aura::Window* const root_window =
        item_view->GetWidget()->GetNativeWindow()->GetRootWindow();
    const std::optional<gfx::Vector2d> offset_to_root_window =
        GetOffsetBetweenLayers(drag_icon_layer, root_window->layer());
    ASSERT_TRUE(offset_to_root_window);
    drag_icon_target_bounds.Offset(*offset_to_root_window);

    // Convert drag icon target bounds to screen.
    gfx::RectF drag_icon_target_bounds_in_screen(drag_icon_target_bounds);
    wm::TranslateRectToScreen(root_window, &drag_icon_target_bounds_in_screen);

    EXPECT_EQ(gfx::RectF(final_item_icon_bounds),
              drag_icon_target_bounds_in_screen);
  }));

  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest,
       DragIconAnimatesToTargetItemBoundsOnSecondaryScreen) {
  UpdateDisplay("1000x700, 1024x768");
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  // Show the app list on the secondary display.
  GetAppListTestHelper()->Dismiss();
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());

  // Start drag from centerpoint of item_view
  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    const gfx::Point drop_point =
        GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, drop_point, apps_grid_view_, 5 /*steps*/);
  }));

  // End drag, and verify target drop icon bounds.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enable drop animation, as the test is verifying target animated
    // transform/bounds.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    EndDrag();

    gfx::Rect final_item_icon_bounds = item_view->GetIconBounds();
    views::View::ConvertRectToScreen(item_view, &final_item_icon_bounds);

    ui::Layer* const drag_icon_layer = GetDragIconLayer(apps_grid_view_);
    // Get drag icon layer's target position relative to the layer target
    // bounds.
    gfx::Rect drag_icon_target_bounds =
        drag_icon_layer->GetTargetTransform().MapRect(
            gfx::Rect(drag_icon_layer->GetTargetBounds().size()));

    // Convert the drag icon target bounds to the layer of the root window that
    // host the drag icon.
    aura::Window* const root_window =
        item_view->GetWidget()->GetNativeWindow()->GetRootWindow();
    const std::optional<gfx::Vector2d> offset_to_root_window =
        GetOffsetBetweenLayers(drag_icon_layer, root_window->layer());
    ASSERT_TRUE(offset_to_root_window);
    drag_icon_target_bounds.Offset(*offset_to_root_window);

    // Convert drag icon target bounds to screen.
    gfx::RectF drag_icon_target_bounds_in_screen(drag_icon_target_bounds);
    wm::TranslateRectToScreen(root_window, &drag_icon_target_bounds_in_screen);

    EXPECT_EQ(gfx::RectF(final_item_icon_bounds),
              drag_icon_target_bounds_in_screen);
  }));

  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, FolderNotOpenedIfGridHidesDuringIconDrop) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Drag the drag view over another app item to create a new folder.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enable animations, as the test is testing interactions that depend on the
    // animation timing.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    EndDrag();

    CheckHaptickEventsCount(1);
    EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
    ASSERT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));

    // Start typing to close the apps page, and open search results.
    GetEventGenerator()->GestureTapAt(
        search_box_view_->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
    EXPECT_FALSE(IsDragIconAnimatingForGrid(apps_grid_view_));

    // Wait for page switch animation.
    auto* helper = GetAppListTestHelper();
    ui::LayerAnimationStoppedWaiter().Wait(
        helper->GetBubbleAppsPage()->GetPageAnimationLayerForTest());

    ASSERT_FALSE(helper->GetBubbleAppsPage()->GetVisible());
    ASSERT_TRUE(helper->GetBubbleSearchPage()->GetVisible());
  }));

  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  // Verify the folder did not get opened, and that the icon drop animation is
  // no longer running.
  EXPECT_FALSE(GetDragIconLayer(apps_grid_view_));
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_F(AppsGridViewTest, CheckFolderWithMultipleItemsContents) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsInFolder;
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);

  // Open the folder and check it's contents.
  test_api_->Update();
  test_api_->PressItemAt(0);

  EXPECT_EQ(1u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            GetTopLevelItemList()->item_at(0)->GetItemType());
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());
  EXPECT_EQ(4, folder_apps_grid_view()->cols());
  EXPECT_TRUE(folder_apps_grid_view()->IsInFolder());
}

TEST_F(AppsGridViewTest, CreatingFolderRecordsUserAction) {
  base::UserActionTester user_actions;

  // Create two apps, then drag the second on top of the first to create a
  // folder.
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Both items are in the folder.
  AppListItem* item_0 = GetTestModel()->FindItem("Item 0");
  AppListItem* item_1 = GetTestModel()->FindItem("Item 1");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_TRUE(item_1->IsInFolder());

  // User action was recorded.
  EXPECT_EQ(1, user_actions.GetActionCount("AppList_CreateFolder"));
}

TEST_F(AppsGridViewTest, DeletingFolderRecordsUserAction) {
  base::UserActionTester user_actions;

  // Create a single-item folder and open it.
  AppListFolderItem* folder =
      GetTestModel()->CreateSingleItemFolder("folder_id", "Item 0");
  std::string folder_id = folder->id();
  test_api_->Update();
  test_api_->PressItemAt(0);

  // Drag the app out of the folder.
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point empty_space =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height());
    UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
               /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Calculate the coordinates for the drop point. Note that we we are
    // dropping into the app list view not the folder view. The (0,1) spot is
    // empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               /*steps=*/5);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Item is in top-level grid and folder is deleted.
  EXPECT_EQ("Item 0", GetTestModel()->GetModelContent());
  EXPECT_FALSE(GetTestModel()->FindFolderItem(folder_id));

  // User action was recorded.
  EXPECT_EQ(1, user_actions.GetActionCount("AppList_DeleteFolder"));
}

TEST_P(AppsGridViewDragTest, MouseDragItemOutOfFolder) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsInFolder;
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());
  // Drag the first folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point empty_space =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height()
                      /*padding to completely exit folder view*/);
    UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Calculate the coordinates for the drop point. Note that we we are
    // dropping into the app list view not the folder view. The (0,1) spot is
    // empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  AppListItem* item_0 = GetTestModel()->FindItem("Item 0");
  AppListItem* item_1 = GetTestModel()->FindItem("Item 1");
  EXPECT_FALSE(item_0->IsInFolder());
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  EXPECT_EQ(std::string(folder_item->id() + ",Item 0"),
            GetTestModel()->GetModelContent());
  EXPECT_EQ(kTotalItems - 1, folder_item->ChildItemCount());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragOutOfFolder) {
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  test_api_->Update();
  test_api_->PressItemAt(0);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point empty_space =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height()
                      /*padding to completely exit folder view*/);
    UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Calculate the coordinates for the drop point. Note that we we are
    // dropping into the app list view not the folder view. The (0,1) spot is
    // empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterDragToAnotherFolder) {
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  test_api_->Update();
  test_api_->PressItemAt(0);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point empty_space =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height()
                      /*padding to completely exit folder view*/);
    UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Calculate the coordinates for the drop point.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  ASSERT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  ui::Layer* drag_icon_layer = GetDragIconLayer(apps_grid_view_);
  ASSERT_TRUE(drag_icon_layer);
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(drag_icon_layer);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest,
       DragIconAnimatesAfterDragThatDeletesOriginalFolder) {
  GetTestModel()->PopulateApps(2);
  GetTestModel()->CreateSingleItemFolder("folder_id", "item_id");
  test_api_->Update();
  test_api_->PressItemAt(2);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the only folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point empty_space =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height()
                      /*padding to completely exit folder view*/);
    UpdateDrag(AppsGridView::MOUSE, empty_space, folder_apps_grid_view(),
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Calculate the coordinates for the drop point. Note that we we are
    // dropping into the app list view not the folder view. The (0,3) spot is
    // empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragIconAnimatesAfterReorderDrag) {
  GetTestModel()->PopulateApps(3);
  test_api_->Update();
  UpdateLayout();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first item to an empty slot in the grid.

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, drop_point, apps_grid_view_, 5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_TRUE(IsDragIconAnimatingForGrid(apps_grid_view_));
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolder) {
  // Create and add an almost full folder.
  const size_t kTotalItems = kMaxItemsInFolder - 1;
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);
  ASSERT_FALSE(folder_item->IsFolderFull());
  // Create and add another item.
  GetTestModel()->PopulateAppWithId(kTotalItems);
  UpdateLayout();

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Dragging one item into the folder, the folder should accept the item.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->LayoutToIdealBounds();
  CheckHaptickEventsCount(1);

  EXPECT_EQ(1u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(folder_item->id(), GetTopLevelItemList()->item_at(0)->id());
  EXPECT_EQ(kTotalItems + 1, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->IsFolderFull());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

TEST_P(AppsGridViewDragTest, MouseDragExceedMaxItemsInFolder) {
  // Create and add a full folder.
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  EXPECT_TRUE(folder_item->IsFolderFull());

  // Create and add another 2 item.
  GetTestModel()->PopulateAppWithId(kMaxItemsInFolder + 1);
  UpdateLayout();

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Dragging the last item over the folder, the folder won't accept the new
    // item.
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->LayoutToIdealBounds();
  CheckHaptickEventsCount(1);

  EXPECT_EQ(2u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(kMaxItemsInFolder, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->IsFolderFull());
}

TEST_P(AppsGridViewDragTest, MouseDragMovement) {
  // Create and add a full folder.
  GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  // Create and add another item.
  GetTestModel()->PopulateAppWithId(kMaxItemsInFolder);
  UpdateLayout();
  AppListItemView* folder_view =
      GetItemViewForPoint(GetItemRectOnCurrentPageAt(0, 0).CenterPoint());
  // Drag the new item to the left so that the grid reorders.
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).bottom_left();
    to.Offset(0, -1);  // Get a point inside the rect.
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
    test_api_->LayoutToIdealBounds();

    // The grid now looks like | blank | folder |.
    EXPECT_EQ(nullptr, GetItemViewForPoint(
                           GetItemRectOnCurrentPageAt(0, 0).CenterPoint()));
    EXPECT_EQ(folder_view, GetItemViewForPoint(
                               GetItemRectOnCurrentPageAt(0, 1).CenterPoint()));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);
}

// Check that moving items around doesn't allow a drop to happen into a full
// folder.
TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolderWithMovement) {
  // Create and add a full folder.
  GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(GetTopLevelItemList()->item_at(0));
  // Create and add another item.
  GetTestModel()->PopulateAppWithId(kMaxItemsInFolder);
  UpdateLayout();
  // Drag the new item to the left so that the grid reorders.
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).bottom_left();
    to.Offset(0, -1);  // Get a point inside the rect.
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point folder_in_second_slot =
        GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, folder_in_second_slot, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  // The item should not have moved into the folder.
  EXPECT_EQ(2u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(kMaxItemsInFolder, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

// Dragging an item towards its neighbours should not reorder until the drag is
// past the folder drop point.
TEST_P(AppsGridViewDragTest, MouseDragItemReorderBeforeFolderDropPoint) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                   GetItemRectOnCurrentPageAt(0, 0).x()) /
                          2;
    gfx::Vector2d drag_vector(-half_tile_width - 4, 0);
    // Flip drag vector in rtl.
    if (is_rtl_) {
      drag_vector.set_x(-drag_vector.x());
    }

    // Drag left but stop before the folder dropping circle.
    UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 1"), GetTestModel()->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderAfterFolderDropPoint) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                   GetItemRectOnCurrentPageAt(0, 0).x()) /
                          2;
    gfx::Vector2d drag_vector(
        -2 * half_tile_width - GetAppListConfig()->folder_bubble_radius() - 4,
        0);
    // Flip drag vector in rtl.
    if (is_rtl_) {
      drag_vector.set_x(-drag_vector.x());
    }

    // Drag left, past the folder dropping circle.
    UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 1,Item 0"), GetTestModel()->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragDownOneRow) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  GetTestModel()->PopulateApps(7);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
    int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                   GetItemRectOnCurrentPageAt(0, 0).x()) /
                          2;
    int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                      GetItemRectOnCurrentPageAt(0, 0).y();
    gfx::Vector2d drag_vector(-half_tile_width, tile_height);
    // Flip drag vector in rtl.
    if (is_rtl_) {
      drag_vector.set_x(-drag_vector.x());
    }

    // Drag down, between apps 5 and 6. The gap should open up, making space for
    // app 1 in the bottom left.
    UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 1,Item 6"),
            GetTestModel()->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragUpOneRow) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  GetTestModel()->PopulateApps(7);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(1, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Point to = GetItemRectOnCurrentPageAt(1, 0).CenterPoint();
    int half_tile_width = std::abs(GetItemRectOnCurrentPageAt(0, 1).x() -
                                   GetItemRectOnCurrentPageAt(0, 0).x()) /
                          2;
    int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                      GetItemRectOnCurrentPageAt(0, 0).y();
    gfx::Vector2d drag_vector(half_tile_width, -tile_height);
    // Flip drag vector in rtl.
    if (is_rtl_) {
      drag_vector.set_x(-drag_vector.x());
    }

    // Drag up, between apps 0 and 2. The gap should open up, making space for
    // app 1 in the top right.
    UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->LayoutToIdealBounds();
  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 5,Item 1,Item 2,Item 3,Item 4,Item 6"),
            GetTestModel()->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewDragTest, MouseDragItemReorderDragPastLastApp) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  GetTestModel()->PopulateApps(7);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

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
    if (is_rtl_) {
      drag_vector.set_x(-drag_vector.x());
    }

    UpdateDrag(AppsGridView::MOUSE, to + drag_vector, apps_grid_view_,
               10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 6,Item 1"),
            GetTestModel()->GetModelContent());
  TestAppListItemViewIndice();
}

// Dragging folder over item_2 should leads to re-ordering these two items.
TEST_P(AppsGridViewDragTest, MouseDragFolderOverItemReorder) {
  size_t kTotalItems = 2;
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);
  GetTestModel()->PopulateAppWithId(kTotalItems);
  UpdateLayout();

  // TODO(anasalazar): Investigate why Mouse pointer does not
  // ExceedDragThresehold in this case to trigger drag.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    UpdateDrag(AppsGridView::TOUCH, to, apps_grid_view_);
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  test_api_->LayoutToIdealBounds();
  CheckHaptickEventsCount(0);

  EXPECT_EQ(2u, GetTopLevelItemList()->item_count());
  EXPECT_EQ("Item 2", GetTopLevelItemList()->item_at(0)->id());
  EXPECT_EQ(folder_item->id(), GetTopLevelItemList()->item_at(1)->id());
  TestAppListItemViewIndice();
}

// Canceling drag should keep existing order.
TEST_P(AppsGridViewDragTest, MouseDragWithCancelKeepsOrder) {
  size_t kTotalItems = 2;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Dismiss the app list to cancel drag.
    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->ShowAppList();
    CheckHaptickEventsCount(1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Needed by the controller
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_EQ(std::string("Item 0,Item 1"), GetTestModel()->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Deleting an item keeps remaining intact.
TEST_P(AppsGridViewDragTest, MouseDragWithDeleteItemKeepsOrder) {
  size_t kTotalItems = 3;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetTestModel()->DeleteItem(GetTestModel()->GetItemName(2)); }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 1"), GetTestModel()->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Adding a launcher item cancels the drag and respects the order.
TEST_P(AppsGridViewDragTest, MouseDragWithAddItemKeepsOrder) {
  size_t kTotalItems = 2;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetTestModel()->CreateAndAddItem("Extra"); }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  EXPECT_EQ(std::string("Item 0,Item 1,Extra"),
            GetTestModel()->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Regression test for crash bug. https://crbug.com/1166011.
TEST_P(AppsGridViewClamshellAndTabletTest, MoveItemInModelPastEndOfList) {
  GetTestModel()->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // I speculate that the item list is missing an item, but PagedViewStructure
  // doesn't know about it. This could happen if an item was deleted during a
  // period that AppsGridView was not observing the list.
  AppListItemList* item_list = GetTopLevelItemList();
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
  GetTestModel()->PopulateApps(20);
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
  GetTestModel()->PopulateApps(20);
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
  GetTestModel()->PopulateApps(20);
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

// Test that the keyboard actions are no-op while a drag is active.
TEST_P(AppsGridViewClamshellAndTabletTest, ControlArrowIsNoOpDuringDrag) {
  base::HistogramTester histogram_tester;
  GetTestModel()->PopulateApps(20);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* item_view =
      GetItemViewInCurrentPageAt(0, 0, apps_grid_view_);

  StartDragForViewAndFireTimer(AppsGridView::TOUCH, item_view);
  AppListItemView* moving_item = GetItemViewInTopLevelGrid(1);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);
    EXPECT_EQ(item_view, test_api_->GetViewAtIndex(GridIndex(0, 0)));
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(0, 1)));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);
    EXPECT_EQ(item_view, test_api_->GetViewAtIndex(GridIndex(0, 0)));
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(0, 1)));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);
    EXPECT_EQ(item_view, test_api_->GetViewAtIndex(GridIndex(0, 0)));
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(0, 1)));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);
    EXPECT_EQ(item_view, test_api_->GetViewAtIndex(GridIndex(0, 0)));
    EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(0, 1)));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End the drag to satisfy checks in AppsGridView destructor.
    EndDrag(AppsGridView::TOUCH);
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
  histogram_tester.ExpectBucketCount(GetItemMoveTypeHistogramName(), 6, 0);
}

// Tests that an item is scrolled to visible position if moved to initially
// invisible grid slot, and that histograms for a long keyboard sequence move
// gets recorded only once.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDownwardMoveSequenceToSlotNotInitiallyVisible) {
  base::HistogramTester histogram_tester;
  GetTestModel()->PopulateApps(40);
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
  GetTestModel()->PopulateApps(apps_grid_view_->cols() + 1);

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
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) +
                               (kPages - 1) * GetTilesPerPageInPagedGrid(1));

  // For every item in the first row, ensure an upward move results in the item
  // swapping places with the item directly above it.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    GetPaginationModel()->SelectPage(1, false /*animate*/);
    const GridIndex moved_view_index(1, i);
    apps_grid_view_->GetFocusManager()->SetFocusedView(
        test_api_->GetViewAtIndex(moved_view_index));

    const GridIndex swapped_view_index(
        0, GetTilesPerPageInPagedGrid(0) - apps_grid_view_->cols() + i);
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
        0, GetTilesPerPageInPagedGrid(0) - apps_grid_view_->cols() + i);
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
  GridIndex moved_view_index(0, GetTilesPerPageInPagedGrid(0) - 1);
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

// Tests that moving a lonely app on the last page down is a no-op when there
// are no pages below.
TEST_P(AppsGridViewClamshellAndTabletTest,
       ControlArrowDownOnLastAppOnLastPage) {
  base::HistogramTester histogram_tester;
  const int kItemCount =
      paged_apps_grid_view_ ? GetTilesPerPageInPagedGrid(0) + 1 : 21;
  GetTestModel()->PopulateApps(kItemCount);
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

// Tests that control + shift + arrow puts |selected_item_| into a folder or
// creates a folder if one does not exist.
TEST_P(AppsGridViewClamshellAndTabletTest, ControlShiftArrowFoldersItemBasic) {
  base::HistogramTester histogram_tester;
  GetTestModel()->PopulateApps(3 * apps_grid_view_->cols());
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

  // The folder is expected to get opened after creation.
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Folder name view has focus.
  EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
  EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                  ->GetFolderNameViewForTest()
                  ->HasFocus());

  // Close the folder.
  event_generator->PressAndReleaseKey(ui::VKEY_UP);
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());

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
  GetTestModel()->PopulateApps(3 * apps_grid_view_->cols());
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

  // The folder is expected to get opened after creation.
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Folder name has focus.
  EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
  EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                  ->GetFolderNameViewForTest()
                  ->HasFocus());

  // Close the folder.
  event_generator->PressAndReleaseKey(ui::VKEY_UP);
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(expected_folder_view_bounds, new_folder->GetBoundsInScreen());

  ASSERT_TRUE(new_folder->HasFocus());
  ASSERT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
}

// Tests that foldering an item that is on a different page fails.
TEST_P(AppsGridViewTabletTest, ControlShiftArrowFailsToFolderAcrossPages) {
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) +
                               GetTilesPerPageInPagedGrid(1));
  UpdateLayout();

  // For every item on the last row of the first page, test that foldering to
  // the next page fails.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    const GridIndex moved_view_index(
        0, GetTilesPerPageInPagedGrid(0) - apps_grid_view_->cols() + i);
    AppListItemView* attempted_folder_view =
        test_api_->GetViewAtIndex(moved_view_index);
    apps_grid_view_->GetFocusManager()->SetFocusedView(attempted_folder_view);

    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

    EXPECT_EQ(attempted_folder_view,
              test_api_->GetViewAtIndex(moved_view_index));
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
  }

  {
    // The last item on the col is selected, try moving right and test that that
    // fails as well.
    GridIndex moved_view_index(0, GetTilesPerPageInPagedGrid(0) - 1);
    AppListItemView* attempted_folder_view =
        test_api_->GetViewAtIndex(moved_view_index);

    SimulateKeyPress(is_rtl_ ? ui::VKEY_LEFT : ui::VKEY_RIGHT,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

    EXPECT_EQ(attempted_folder_view,
              test_api_->GetViewAtIndex(moved_view_index));

    // Move to the second page and test that foldering up to a new page fails.
    SimulateKeyPress(ui::VKEY_DOWN);

    // Select the first item on the second page.
    moved_view_index = GridIndex(1, 0);
    attempted_folder_view = test_api_->GetViewAtIndex(moved_view_index);

    // Try to folder left to the previous page, it  should fail.
    SimulateKeyPress(is_rtl_ ? ui::VKEY_RIGHT : ui::VKEY_LEFT,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

    EXPECT_EQ(attempted_folder_view,
              test_api_->GetViewAtIndex(moved_view_index));
  }

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
  const int kTopLevelItemCount =
      paged_apps_grid_view_
          ? GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1)
          : apps_grid_view_->cols() * 8;
  GetTestModel()->PopulateApps(kTopLevelItemCount - 1);
  const AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
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
  ASSERT_EQ(folder_item, GetTestModel()->FindItem(folder_id));
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
  const int kTopLevelItemCount =
      paged_apps_grid_view_
          ? GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1)
          : apps_grid_view_->cols() * 8;
  GetTestModel()->PopulateApps(kTopLevelItemCount - 1);
  const AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(3);
  const std::string folder_id = folder_item->id();
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
  ASSERT_EQ(folder_item, GetTestModel()->FindItem(folder_id));
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
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) - 2);
  const AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(3);
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(1));
  const std::string folder_id = folder_item->id();
  UpdateLayout();

  AppListItemView* folder_view = test_api_->GetViewAtIndex(
      GridIndex(0, GetTilesPerPageInPagedGrid(0) - 2));
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
  EXPECT_EQ("Item " + base::NumberToString(GetTilesPerPageInPagedGrid(0) - 2),
            reparented_item_id);
  ASSERT_TRUE(reparented_item_view->HasFocus());

  // Reparent the item to the slot after the folder view, which should be the
  // last spot in the grid.
  event_generator->PressAndReleaseKey(ui::VKEY_DOWN,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_EQ(folder_item, GetTestModel()->FindItem(folder_id));
  EXPECT_EQ(2u, folder_item->ChildItemCount());

  const AppListItemView* last_item_on_first_page = test_api_->GetViewAtIndex(
      GridIndex(0, GetTilesPerPageInPagedGrid(0) - 1));
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
  GetTestModel()->PopulateApps(kNumberOfApps);
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

  // The folder is expected to get opened after creation.
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Folder name view has focus.
  EXPECT_EQ(folder_item, app_list_folder_view_->folder_item());
  EXPECT_TRUE(app_list_folder_view_->folder_header_view()
                  ->GetFolderNameViewForTest()
                  ->HasFocus());

  // Close the folder.
  event_generator->PressAndReleaseKey(ui::VKEY_UP);
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  EXPECT_TRUE(new_folder->HasFocus());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       MoveLastItemFromFolderToRightDoesNotCrash) {
  ui::test::EventGenerator* const event_generator = GetEventGenerator();

  // Create a folder with two items in it.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  AppListItemView* folder_view = test_api_->GetViewAtIndex(GridIndex(0, 0));

  // Open the folder.
  folder_view->RequestFocus();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // An item inside the folder should be in focus. Move it to the left to put it
  // before the folder icon.
  event_generator->PressAndReleaseKey(ui::VKEY_LEFT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(test_api_->GetViewAtIndex(GridIndex(0, 0))->HasFocus());

  // Open the folder again.
  folder_view->RequestFocus();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // An item inside the folder should be in focus. Move it to the right to put
  // it after/instead the folder icon. No crash happens.
  event_generator->PressAndReleaseKey(ui::VKEY_RIGHT,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(test_api_->GetViewAtIndex(GridIndex(0, 1))->HasFocus());
}

TEST_P(AppsGridViewTabletDragTest, TouchDragFlipToNextPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 3 full pages of apps.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) +
                               GetTilesPerPageInPagedGrid(1) +
                               GetTilesPerPageInPagedGrid(2));
  UpdateLayout();

  const gfx::Rect apps_grid_bounds = paged_apps_grid_view_->GetBoundsInScreen();
  // Drag an item to the bottom to start flipping pages.
  page_flip_waiter_->Reset();

  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    gfx::Point point_in_page_flip_buffer = apps_grid_bounds.bottom_center();
    point_in_page_flip_buffer.Offset(0, -1);
    UpdateDragInScreen(AppsGridView::TOUCH, point_in_page_flip_buffer,
                       5 /*steps*/);
    while (HasPendingPageFlip(paged_apps_grid_view_)) {
      page_flip_waiter_->Wait();
    }

    // A new page cannot be created or flipped to.
    EXPECT_EQ("1,2", page_flip_waiter_->selected_pages());
    EXPECT_EQ(2, GetPaginationModel()->selected_page());

    // The drag is centered relative to the app item icon bounds, not the whole
    // app item view. Account for the scale factor of the app icon during drag.
    gfx::Vector2d icon_offset(
        0, std::round(GetAppListConfig()->grid_icon_bottom_padding() *
                      kDragDropAppIconScale) /
               2);
    EXPECT_LE(
        2, CalculateManhattanDistance(point_in_page_flip_buffer - icon_offset,
                                      GetDragIconCenter()));
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
  CheckHaptickEventsCount(0);
}

TEST_P(AppsGridViewTabletTestWithDragAndDropRefactor, ReparentDragToNewPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  GetTestModel()->CreateAndPopulateFolderWithApps(3);
  // Fill up the first page.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) - 1);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag an item from the first page to the last existing slot on the next
  // page.
  AppListItemView* dragged_view =
      folder_apps_grid_view()->view_model()->view_at(0);
  const std::string dragged_view_id = dragged_view->item()->id();
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_view);

  CheckHaptickEventsCount(1);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point point_outside_folder =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(10, 10);
    views::View::ConvertPointToScreen(app_list_folder_view(),
                                      &point_outside_folder);
    UpdateDragInScreen(AppsGridView::MOUSE, point_outside_folder,
                       /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    test_api_->WaitForItemMoveAnimationDone();

    // Reparent drag temporarily adds an extra slot to the apps grid, which
    // should create an extra page.
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move mouse to the bottom into the page flip zone.
    gfx::Point page_flip_zone =
        paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, -1);
    UpdateDragInScreen(AppsGridView::MOUSE, page_flip_zone,
                       /*steps=*/10);
    ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));
    page_flip_waiter_->Wait();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item to an empty slot on the second page. Use the second slot as
    // the first one may be occupied by an extra app from he previous step.
    gfx::Point empty_slot =
        test_api_->GetItemTileRectAtVisualIndex(1, 1).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, empty_slot, paged_apps_grid_view_,
               /*steps=*/10);
    if (paged_apps_grid_view_->reorder_timer_for_test()->IsRunning()) {
      paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    }
    test_api_->WaitForItemMoveAnimationDone();
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::MOUSE); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the last slot.
  AppListItemView* last_item_view = test_api_->GetViewAtVisualIndex(1, 0);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(dragged_view_id, last_item_view->item()->id());
}

TEST_P(AppsGridViewTabletTestWithDragAndDropRefactor,
       ReparentDragToAFolderOnNewPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  GetTestModel()->CreateAndPopulateFolderWithApps(3);
  // Fill up the first page, with a folder in the last slot.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) - 2);
  AppListFolderItem* trailing_folder =
      GetTestModel()->CreateAndPopulateFolderWithApps(2);
  const std::string trailing_folder_id = trailing_folder->id();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag an item from the first page to the last existing slot on the next
  // page.
  AppListItemView* dragged_view =
      folder_apps_grid_view()->view_model()->view_at(0);
  const std::string dragged_view_id = dragged_view->item()->id();

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_view);

  CheckHaptickEventsCount(1);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point point_outside_folder =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(10, 10);
    views::View::ConvertPointToScreen(app_list_folder_view(),
                                      &point_outside_folder);
    UpdateDragInScreen(AppsGridView::MOUSE, point_outside_folder,
                       /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    test_api_->WaitForItemMoveAnimationDone();

    // Reparent drag temporarily adds an extra slot to the apps grid, which
    // should create an extra page.
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move mouse to the bottom into the page flip zone.
    gfx::Point page_flip_zone =
        paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, -1);
    UpdateDragInScreen(AppsGridView::MOUSE, page_flip_zone,
                       /*steps=*/10);
    ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));
    page_flip_waiter_->Wait();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item on top of the folder in the first slot on the page.
    gfx::Point trailing_slot =
        test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, trailing_slot, paged_apps_grid_view_,
               /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  // The item was moved to another folder, so the number of pages should have
  // dropped back to 1.
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the last slot.
  AppListItemView* last_item_view =
      test_api_->GetViewAtVisualIndex(0, GetTilesPerPageInPagedGrid(0) - 1);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(trailing_folder_id, last_item_view->item()->id());
  const AppListItem* const dragged_item =
      GetTestModel()->FindItem(dragged_view_id);
  ASSERT_TRUE(dragged_item);
  EXPECT_EQ(trailing_folder_id, dragged_item->folder_id());
}

TEST_P(AppsGridViewTabletTest, DragAcrossPagesToTheLastSlot) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a full page and a partially full second page.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) + 3);
  UpdateLayout();

  // Drag an item from the first page to the last existing slot on the next
  // page.
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  AppListItemView* dragged_view = view_model->view_at(0);
  AppListItemView* original_first_item_on_second_page =
      view_model->view_at(GetTilesPerPageInPagedGrid(0));

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_view);

  CheckHaptickEventsCount(1);

  // Task to move mouse from the page flip area after the page gets flipped, to
  // prevent subseuquent page flips.
  PostPageFlipTask task(GetPaginationModel(), base::BindLambdaForTesting([&]() {
                          GetEventGenerator()->MoveMouseBy(0, -50);
                          GetEventGenerator()->MoveMouseBy(0, -50);
                        }));
  const int expected_final_slot = 2;

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point to = paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
                    gfx::Vector2d(0, -1);
    UpdateDragInScreen(AppsGridView::MOUSE, to,
                       /*steps=*/10);
    ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    page_flip_waiter_->Wait();

    // Ensure that the reorder timer ran, and that any views on the second page
    // that should have been moved to the first page have done so.
    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    test_api_->WaitForItemMoveAnimationDone();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item to the first empty slot on the second page.
    gfx::Point empty_slot =
        test_api_->GetItemTileRectAtVisualIndex(1, 3).CenterPoint();
    views::View::ConvertPointToScreen(paged_apps_grid_view_, &empty_slot);
    UpdateDrag(AppsGridView::MOUSE, empty_slot, paged_apps_grid_view_,
               /*steps=*/10);

    if (paged_apps_grid_view_->reorder_timer_for_test()->IsRunning()) {
      paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    }
    test_api_->WaitForItemMoveAnimationDone();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
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
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the last slot.
  AppListItemView* last_item_view =
      test_api_->GetViewAtVisualIndex(1, expected_final_slot);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(dragged_view->item()->id(), last_item_view->item()->id());

  // The first item on second page should have been moved to the first page (to
  // fill up the empty slot left by moving the draggged item away).
  AppListItemView* last_item_on_first_page =
      test_api_->GetViewAtVisualIndex(0, GetTilesPerPageInPagedGrid(0) - 1);
  ASSERT_TRUE(last_item_on_first_page);
  EXPECT_EQ(original_first_item_on_second_page->item()->id(),
            last_item_on_first_page->item()->id());
}

TEST_P(AppsGridViewTabletTest, DragAcrossPagesToSecondToLastSlot) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a full page and a partially full second page.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) + 3);
  UpdateLayout();

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  AppListItemView* dragged_view = view_model->view_at(0);
  AppListItemView* original_first_item_on_second_page =
      view_model->view_at(GetTilesPerPageInPagedGrid(0));

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_view);

  CheckHaptickEventsCount(1);

  // Task to move mouse from the page flip area after the page gets flipped, to
  // prevent subseuquent page flips.
  PostPageFlipTask task(GetPaginationModel(), base::BindLambdaForTesting([&]() {
                          GetEventGenerator()->MoveMouseBy(0, -50);
                          GetEventGenerator()->MoveMouseBy(0, -50);
                        }));
  const int expected_final_slot = 1;

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the item to launcher page flip zone, and flip the launcher to the
    // second page.
    gfx::Point to = paged_apps_grid_view_->GetBoundsInScreen().bottom_center() +
                    gfx::Vector2d(0, -1);
    UpdateDragInScreen(AppsGridView::MOUSE, to,
                       /*steps=*/10);
    ASSERT_TRUE(HasPendingPageFlip(paged_apps_grid_view_));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    page_flip_waiter_->Wait();

    // Ensure that the reorder timer ran, and that any views on the second page
    // that should have been moved to the first page have done so.
    ASSERT_TRUE(paged_apps_grid_view_->reorder_timer_for_test()->IsRunning());
    paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    test_api_->WaitForItemMoveAnimationDone();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the item between two last slots on the page.
    views::View* last_view = view_model->view_at(view_model->view_size() - 1);
    const gfx::Point last_slot = last_view->GetBoundsInScreen().CenterPoint();
    views::View* second_to_last_view =
        view_model->view_at(view_model->view_size() - 2);
    const gfx::Point second_to_last_slot =
        second_to_last_view->GetBoundsInScreen().CenterPoint();
    const gfx::Point drop_point((last_slot.x() + second_to_last_slot.x()) / 2,
                                last_slot.y());
    UpdateDragInScreen(AppsGridView::MOUSE, drop_point,
                       /*steps=*/10);

    if (paged_apps_grid_view_->reorder_timer_for_test()->IsRunning()) {
      paged_apps_grid_view_->reorder_timer_for_test()->FireNow();
    }
    test_api_->WaitForItemMoveAnimationDone();
    EXPECT_EQ(GridIndex(1, expected_final_slot),
              paged_apps_grid_view_->reorder_placeholder());

    // Verify that the last item in the grid is right of the expected
    // placeholder location.
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
      views::View::ConvertPointToTarget(
          second_to_last_view, paged_apps_grid_view_,
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
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);

  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();

  // Verify that the dragged item was moved to the target slot.
  AppListItemView* last_item_view =
      test_api_->GetViewAtVisualIndex(1, expected_final_slot);
  ASSERT_TRUE(last_item_view);
  EXPECT_EQ(dragged_view->item()->id(), last_item_view->item()->id());

  // The first item on second page should have been moved to the first page (to
  // fill up the empty slot left by moving the draggged item away).
  AppListItemView* last_item_on_first_page =
      test_api_->GetViewAtVisualIndex(0, GetTilesPerPageInPagedGrid(0) - 1);
  ASSERT_TRUE(last_item_on_first_page);
  EXPECT_EQ(original_first_item_on_second_page->item()->id(),
            last_item_on_first_page->item()->id());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeOverflownLandspaceToPortait) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const size_t kTotalApps =
      GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1) + 1;
  GetTestModel()->PopulateApps(kTotalApps);
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages decreased if new
  // page structure fit all apps into 2 pages (number of items per page may
  // change between landscape and portrait mode).
  UpdateDisplay("1024x768/r");

  EXPECT_EQ(kTotalApps <= GetTilesPerPageInPagedGrid(0) +
                              GetTilesPerPageInPagedGrid(1)
                ? 2
                : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeUnderflowLandspaceToPortait) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const size_t kTotalApps =
      GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1) - 1;
  GetTestModel()->PopulateApps(kTotalApps);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages increased if new
  // page structure does not fit all apps into 2 pages (number of items per page
  // may change between landscape and portrait mode).
  UpdateDisplay("1024x768/r");

  EXPECT_EQ(kTotalApps <= GetTilesPerPageInPagedGrid(0) +
                              GetTilesPerPageInPagedGrid(1)
                ? 2
                : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeOverflownPortraitToLandcape) {
  ASSERT_TRUE(paged_apps_grid_view_);
  UpdateDisplay("1024x768/r");

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const size_t kTotalApps =
      GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1) + 1;
  GetTestModel()->PopulateApps(kTotalApps);
  EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages decreased if new
  // page structure fit all apps into 2 pages (number of items per page may
  // change between landscape and portrait mode).
  UpdateDisplay("1024x768");

  EXPECT_EQ(kTotalApps <= GetTilesPerPageInPagedGrid(0) +
                              GetTilesPerPageInPagedGrid(1)
                ? 2
                : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletTest,
       UpdatePagingIfPageSizesChangeUnderflowPortraitToLandscape) {
  ASSERT_TRUE(paged_apps_grid_view_);
  UpdateDisplay("1024x768/r");

  // Create 2 full pages of apps, and add another app to overflow to third page.
  const size_t kTotalApps =
      GetTilesPerPageInPagedGrid(0) + GetTilesPerPageInPagedGrid(1) - 1;
  GetTestModel()->PopulateApps(kTotalApps);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Rotate the screen, and verify that the number of pages increased if new
  // page structure fits all apps into 2 pages (portrait mode grid has more
  // items per page than landscape UI).
  UpdateDisplay("1024x768");

  EXPECT_EQ(kTotalApps <= GetTilesPerPageInPagedGrid(0) +
                              GetTilesPerPageInPagedGrid(1)
                ? 2
                : 3,
            GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTabletDragTest, TouchDragFlipToPreviousPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create 3 full pages of apps.
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) +
                               GetTilesPerPageInPagedGrid(1) +
                               GetTilesPerPageInPagedGrid(2));
  // Select the last page.
  GetPaginationModel()->SelectPage(2, /*animate=*/false);

  // Drag an item to the top to start flipping pages.
  page_flip_waiter_->Reset();
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    const gfx::Rect apps_grid_bounds =
        paged_apps_grid_view_->GetBoundsInScreen();
    gfx::Point point_in_page_flip_buffer = apps_grid_bounds.top_center();
    point_in_page_flip_buffer.Offset(0, 10);
    UpdateDragInScreen(AppsGridView::TOUCH, point_in_page_flip_buffer,
                       /*steps=*/5);
    while (HasPendingPageFlip(paged_apps_grid_view_)) {
      page_flip_waiter_->Wait();
    }

    // We flipped back to the first page.
    EXPECT_EQ("1,0", page_flip_waiter_->selected_pages());
    EXPECT_EQ(0, GetPaginationModel()->selected_page());

    // The drag is centered relative to the app item icon bounds, not the whole
    // app item view. Account for the scale factor of the app icon during drag.
    gfx::Vector2d icon_offset(
        0, std::round(GetAppListConfig()->grid_icon_bottom_padding() *
                      kDragDropAppIconScale) /
               2);
    EXPECT_LE(
        2, CalculateManhattanDistance(point_in_page_flip_buffer - icon_offset,
                                      GetDragIconCenter()));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End the drag to satisfy checks in AppsGridView destructor.
    EndDrag(AppsGridView::TOUCH);
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
  CheckHaptickEventsCount(0);
}

TEST_P(AppsGridViewDragTest, CancelDragDoesNotReorderItems) {
  const int kTotalItems = 4;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  ASSERT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            GetTestModel()->GetModelContent());

  // Starts a mouse drag and then cancels it.
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    const gfx::Point to = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->ShowAppList();

    CheckHaptickEventsCount(1);

    // Model is not changed.
    EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
              GetTestModel()->GetModelContent());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Required by the controller to release pointer.
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

// Test focus change before dragging an item. (See https://crbug.com/834682)
TEST_F(AppsGridViewTest, FocusOfDraggedViewBeforeDrag) {
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
  EXPECT_FALSE(apps_grid_view_->view_model()->view_at(0)->HasFocus());
}

// Test focus change during dragging an item. (See https://crbug.com/834682)
TEST_P(AppsGridViewDragTest, FocusOfDraggedViewDuringDrag) {
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  AppListItemView* item_view =
      GetItemViewInCurrentPageAt(0, 0, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    const gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    // Dragging the item towards its right.
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);

    EXPECT_FALSE(search_box_view_->search_box()->HasFocus());
    EXPECT_TRUE(item_view->HasFocus());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

// Test focus change after dragging an item. (See https://crbug.com/834682)
TEST_P(AppsGridViewDragTest, FocusOfDraggedViewAfterDrag) {
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  auto* item_view = apps_grid_view_->view_model()->view_at(0);
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    const gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // The search box keeps focus after drags.
  EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
  EXPECT_FALSE(item_view->HasFocus());
}

TEST_P(AppsGridViewDragTest, FocusOfReparentedDragViewWithFolderDeleted) {
  // Creates a folder item with two items.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(1);
  test_api_->Update();

  // Leave the dragged item as a single folder child.
  GetTestModel()->DeleteItem("Item 1");
  // One folder and one app. Therefore the top level view count is 2.
  EXPECT_EQ(2u, apps_grid_view_->view_model()->view_size());

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag the first folder child out of the folder.
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE,
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view()));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point point_outside_folder =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(10, 10);
    UpdateDrag(AppsGridView::MOUSE, point_outside_folder,
               folder_apps_grid_view(),
               /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drop the item in (0,2) spot is the root apps grid. The spot is expected
    // to be empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               /*steps=*/5);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    BoundsChangeCounter counter(GetItemViewInTopLevelGrid(1));
    EndDrag();

    // Verify that Item 2's bounds change after calling `EndDrag()`.
    EXPECT_EQ(1, counter.bounds_change_count());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // The folder should be deleted. The first item should be Item 2, the second
  // item should be Item 0.
  EXPECT_EQ(2u, apps_grid_view_->view_model()->view_size());
  EXPECT_EQ("Item 2", GetItemViewInTopLevelGrid(0)->item()->id());
  EXPECT_EQ("Item 0", GetItemViewInTopLevelGrid(1)->item()->id());

  AppListItemView* const dragged_view = GetItemViewInTopLevelGrid(1);

  // The search box keeps focus after drags.
  EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
  EXPECT_FALSE(dragged_view->HasFocus());
}

TEST_P(AppsGridViewDragTest, FocusOfReparentedDragViewAfterDrag) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  GetTestModel()->PopulateApps(2);
  test_api_->Update();

  // Open the folder.
  test_api_->PressItemAt(0);

  // Drag the first folder child out of the folder.

  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE,
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view()));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point point_outside_folder =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(10, 10);
    UpdateDrag(AppsGridView::MOUSE, point_outside_folder,
               folder_apps_grid_view(),
               /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drop the item in (0,3) spot is the root apps grid. The spot is expected
    // to be empty.
    gfx::Point drop_point = GetItemRectOnCurrentPageAt(0, 3).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               /*steps=*/5);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(3);
  EXPECT_EQ("Item 0", item_view->item()->id());

  // The search box keeps focus after drags.
  EXPECT_TRUE(search_box_view_->search_box()->HasFocus());
  EXPECT_FALSE(item_view->HasFocus());
}

TEST_P(AppsGridViewDragTest, DragAndPinItemToShelf) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Verify that item drag has started.
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Releasing drag over shelf should pin the dragged app.
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_EQ("Item 1", ShelfModel::Get()->items()[0].id.app_id);
  CheckHaptickEventsCount(1);
}

TEST_P(AppsGridViewDragTest, DragAndPinFolderItemToShelf) {
  GetTestModel()->PopulateApps(2);
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(2);
  ASSERT_TRUE(item_view->is_folder());

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Verify that item drag has started.
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(folder_item, apps_grid_view_->drag_item());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_TRUE(shelf_view->drag_and_drop_shelf_id().IsNull());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EXPECT_FALSE(ShelfModel::Get()->IsAppPinned(folder_item->id()));

  // Make sure that the shelf does not have a drag view assigned.
  EXPECT_FALSE(shelf_view->drag_image_layer_for_test());
  EXPECT_FALSE(shelf_view->drag_view());
}

TEST_P(AppsGridViewDragTest, DragAndPinNotInitiallyVisibleItemToShelf) {
  // Add more apps to the root apps grid.
  GetTestModel()->PopulateApps(50);
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

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Verify app list item drag has started.
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 40", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Releasing drag over shelf should pin the dragged app.
  CheckHaptickEventsCount(1);
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 40"));
  EXPECT_EQ("Item 40", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragItemToAndFromShelf) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Verify app list item drag has started.
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
    CheckHaptickEventsCount(1);
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);
    CheckHaptickEventsCount(1);
    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the app away from shelf, and verify the app doesn't get pinned when
    // the drag ends.
    UpdateDragInScreen(AppsGridView::MOUSE,
                       apps_grid_view_->GetBoundsInScreen().origin());

    CheckHaptickEventsCount(1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
  CheckHaptickEventsCount(1);

  EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_TRUE(ShelfModel::Get()->items().empty());
}

TEST_P(AppsGridViewDragTest, DragAndPinItemFromFolderToShelf) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  GetTestModel()->PopulateApps(2);
  test_api_->Update();

  // Open the folder.
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Verify app list item drag has started.
    ASSERT_TRUE(folder_apps_grid_view()->drag_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
    ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

    UpdateDragInScreen(
        AppsGridView::MOUSE,
        app_list_folder_view()->GetBoundsInScreen().right_center() +
            gfx::Vector2d(20, 0),
        /*steps=*/1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Releasing drag over shelf should pin the dragged app.
  CheckHaptickEventsCount(1);
  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_EQ("Item 1", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragAndPinNotInitiallyVisibleFolderItemToShelf) {
  GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
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

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Verify app list item drag has started.
    ASSERT_TRUE(folder_apps_grid_view()->drag_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
    ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

    UpdateDragInScreen(
        AppsGridView::MOUSE,
        app_list_folder_view()->GetBoundsInScreen().right_center() +
            gfx::Vector2d(20, 0),
        /*steps=*/1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 30", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Releasing drag over shelf should pin the dragged app.
  CheckHaptickEventsCount(1);

  EXPECT_TRUE(ShelfModel::Get()->IsAppPinned("Item 30"));
  EXPECT_EQ("Item 30", ShelfModel::Get()->items()[0].id.app_id);
}

TEST_P(AppsGridViewDragTest, DragAnItemFromFolderToAndFromShelf) {
  // Creates a folder item - the folder size was chosen arbitrarily.
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  // Open the folder.
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInCurrentPageAt(0, 1, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    // Verify app list item drag has started.
    ASSERT_TRUE(folder_apps_grid_view()->drag_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
    ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

    UpdateDragInScreen(
        AppsGridView::MOUSE,
        app_list_folder_view()->GetBoundsInScreen().right_center() +
            gfx::Vector2d(20, 0),
        /*steps=*/1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Shelf should start handling the drag if it moves within its bounds.
    auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);
    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the app away from shelf, and verify the app doesn't get pinned when
    // the drag ends.
    UpdateDragInScreen(AppsGridView::MOUSE,
                       apps_grid_view_->GetBoundsInScreen().origin(),
                       /*steps=*/1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EndDrag();
  CheckHaptickEventsCount(1);

  EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
  EXPECT_TRUE(ShelfModel::Get()->items().empty());
}

TEST_P(AppsGridViewDragTest, RemoveDisplayWhileDraggingItemOntoShelf) {
  UpdateDisplay("1024x768,1024x768");
  GetTestModel()->PopulateApps(3);

  // Show the app list on the secondary display.
  GetAppListTestHelper()->Dismiss();
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(1);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Verify that item drag has started.
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    Shelf* const secondary_shelf =
        Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
            ->shelf();

    // Shelf should start handling the drag if it moves within its bounds.
    ShelfView* shelf_view = secondary_shelf->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enable animations to catch potential crashes during display removal.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // Remove display while drag is over the shelf bounds, verify that the shelf
    // model does not change.
    UpdateDisplay("1024x768");
    EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
    EXPECT_TRUE(ShelfModel::Get()->items().empty());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // DragDropController requires the test to release the pointer in order to
    // free the drag loop.
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

TEST_P(AppsGridViewDragTest, RemoveDisplayWhileDraggingFolderItemOntoShelf) {
  UpdateDisplay("1024x768,1024x768");

  // Creates a folder item - the folder size was chosen arbitrarily.
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // Add more apps to the root apps grid.
  GetTestModel()->PopulateApps(2);

  // Show the app list on the secondary display.
  GetAppListTestHelper()->Dismiss();
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());

  // Open the folder.
  test_api_->PressItemAt(0);

  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(1, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Verify app list item drag has started.
    ASSERT_TRUE(folder_apps_grid_view()->drag_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
    ASSERT_EQ(item_view->item(), folder_apps_grid_view()->drag_item());

    UpdateDragInScreen(
        AppsGridView::MOUSE,
        app_list_folder_view()->GetBoundsInScreen().right_center() +
            gfx::Vector2d(20, 0),
        /*steps=*/1);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    Shelf* const secondary_shelf =
        Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
            ->shelf();

    // Shelf should start handling the drag if it moves within its bounds.
    ShelfView* shelf_view = secondary_shelf->GetShelfViewForTesting();
    UpdateDragInScreen(
        AppsGridView::MOUSE,
        shelf_view->GetBoundsInScreen().left_center() + gfx::Vector2d(5, 5),
        /*steps=*/1);

    EXPECT_EQ("Item 1", shelf_view->drag_and_drop_shelf_id().app_id);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enable animations to catch potential crashes during display removal.
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // Remove display while drag is over the shelf bounds, verify that the shelf
    // model does not change.
    UpdateDisplay("1024x768");
    EXPECT_FALSE(ShelfModel::Get()->IsAppPinned("Item 1"));
    EXPECT_TRUE(ShelfModel::Get()->items().empty());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // DragDropController requires the test to release the pointer in order to
    // free the drag loop.
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

TEST_P(AppsGridViewDragTest, MousePointerIsGrabbingDuringDrag) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto previous_cursor_type = cursor_manager->GetCursor().type();

  // Populate the apps grid and start dragging one of the items.
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(0);

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Ensure the cursor type is set to grabbing during the drag.
    EXPECT_EQ(ui::mojom::CursorType::kGrabbing,
              cursor_manager->GetCursor().type());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // Release the left mouse button to cancel the drag and verify that the cursor
  // type is reset.
  EXPECT_EQ(previous_cursor_type, cursor_manager->GetCursor().type());
}

TEST_P(AppsGridViewDragTest, MousePointerIsResetOnCanceledDrag) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto previous_cursor_type = cursor_manager->GetCursor().type();

  // Populate the apps grid and start dragging one of the items.
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  AppListItemView* const item_view = GetItemViewInTopLevelGrid(0);

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // The cursor type should be set to grabbing during the drag.
    ASSERT_EQ(ui::mojom::CursorType::kGrabbing,
              cursor_manager->GetCursor().type());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Cancel the drag without releasing the left mouse button and verify that
    // the cursor is still reset in this case.
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
    EXPECT_EQ(previous_cursor_type, cursor_manager->GetCursor().type());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

// Verify the cursor type when dragging one item over another item and back.
TEST_P(AppsGridViewDragTest, MouseDragItemToOtherItemAndBack) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  // Start dragging the first item.
  AppListItemView* const item0 = GetItemViewInTopLevelGrid(0);
  gfx::Point starting_point = item0->GetBoundsInScreen().CenterPoint();
  AppListItemView* const item1 = GetItemViewInTopLevelGrid(1);

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item0);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Verify the cursor is grabbing now that the drag has started.
    ASSERT_EQ(ui::mojom::CursorType::kGrabbing,
              cursor_manager->GetCursor().type());
    // Move the first item on top of the second item as if to create a folder,
    // but don't actually create a folder.
    UpdateDragInScreen(AppsGridView::MOUSE,
                       item1->GetBoundsInScreen().CenterPoint());
    // Verify the cursor is still grabbing in this state.
    ASSERT_EQ(ui::mojom::CursorType::kGrabbing,
              cursor_manager->GetCursor().type());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the first item back to its original position.
    UpdateDragInScreen(AppsGridView::MOUSE, starting_point);

    // The cursor should still be grabbing.
    EXPECT_EQ(ui::mojom::CursorType::kGrabbing,
              cursor_manager->GetCursor().type());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  test_api_->WaitForItemMoveAnimationDone();
  // Verify the cursor is not grabbing in this state.
  ASSERT_EQ(ui::mojom::CursorType::kNull, cursor_manager->GetCursor().type());
}

TEST_P(AppsGridViewDragTest, NewInstallDotVisibilityDuringDrag) {
  GetTestModel()->PopulateApps(1);
  UpdateLayout();

  // By default, the new install dot is not visible.
  AppListItemView* const item_view = GetItemViewInTopLevelGrid(0);
  ASSERT_FALSE(item_view->item()->is_new_install());
  views::View* new_install_dot = GetNewInstallDot(item_view);
  ASSERT_TRUE(new_install_dot);
  EXPECT_FALSE(new_install_dot->GetVisible());

  // Set the item as a new install to show the new install dot.
  item_view->item()->SetIsNewInstall(true);
  ASSERT_TRUE(new_install_dot->GetVisible());

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);
    EXPECT_FALSE(new_install_dot->GetVisible());

    const gfx::Point to = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_);
    EXPECT_FALSE(new_install_dot->GetVisible());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // When ending drag, the new install dot should be visible again.
  EXPECT_TRUE(new_install_dot->GetVisible());
}

TEST_P(AppsGridViewTabletTest, Basic) {
  base::HistogramTester histogram_tester;

  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0) + 1);
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, -1));
  ui::GestureEvent scroll_update(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, -10));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  // Drag up on the app grid when on page 1, this should move the AppsGridView
  // but not the AppListView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  histogram_tester.ExpectTotalCount(
      "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode", 0);

  apps_grid_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
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
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(GetTilesPerPageInPagedGrid(0));
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  gfx::Point apps_grid_view_origin =
      apps_grid_view_->GetBoundsInScreen().origin();
  ui::GestureEvent scroll_begin(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, -1));
  ui::GestureEvent scroll_update_upwards(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, -10));
  ui::GestureEvent scroll_update_downwards(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, 15));
  ui::GestureEvent scroll_end(
      apps_grid_view_origin.x(), apps_grid_view_origin.y(), 0,
      base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  AppListItemView* folder_view = GetItemViewInTopLevelGrid(0);
  ASSERT_TRUE(folder_view->is_folder());

  views::View* scrollable_container = app_list_view_->app_list_main_view()
                                          ->contents_view()
                                          ->apps_container_view()
                                          ->scrollable_container_for_test();
  ASSERT_TRUE(scrollable_container->layer()->gradient_mask().IsEmpty());

  // On the first page drag upwards, there should not be a page switch and the
  // layer mask should make the folder lose blur.
  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  apps_grid_view_->OnGestureEvent(&scroll_update_upwards);
  EXPECT_TRUE(scroll_update_upwards.handled());

  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  ASSERT_FALSE(scrollable_container->layer()->gradient_mask().IsEmpty());

  // Continue drag, now switching directions and release. There shouldn't be
  // any transition and the mask layer should've been reset.
  apps_grid_view_->OnGestureEvent(&scroll_update_downwards);
  EXPECT_TRUE(scroll_update_downwards.handled());
  apps_grid_view_->OnGestureEvent(&scroll_end);
  EXPECT_TRUE(scroll_end.handled());

  EXPECT_FALSE(GetPaginationModel()->has_transition());
  EXPECT_TRUE(scrollable_container->layer()->gradient_mask().IsEmpty());
}

TEST_P(AppsGridViewClamshellAndTabletTest, PopulateAppsGridWithTwoApps) {
  const int kApps = 2;
  GetTestModel()->PopulateApps(kApps);

  if (create_as_tablet_mode_) {
    // There's only one page and both items are in that page.
    EXPECT_EQ(0, GetPaginationModel()->selected_page());
    EXPECT_EQ(1, GetPaginationModel()->total_pages());
  }
  TestAppListItemViewIndice();
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(2u, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(0)->item()->id());
  EXPECT_EQ(view_model->view_at(1),
            test_api_->GetViewAtVisualIndex(0 /* page */, 1 /* slot */));
  EXPECT_EQ("Item 1", view_model->view_at(1)->item()->id());
  EXPECT_EQ(std::string("Item 0,Item 1"), GetTestModel()->GetModelContent());
}

TEST_F(AppsGridViewTest, PopulateAppsGridWithAFolder) {
  // Creates a folder item.
  const size_t kTotalItems = kMaxItemsInFolder;
  AppListFolderItem* folder_item =
      GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);

  // Open the folder and check it's contents.
  test_api_->Update();
  test_api_->PressItemAt(0);

  EXPECT_EQ(1u, GetTopLevelItemList()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            GetTopLevelItemList()->item_at(0)->GetItemType());
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());
  EXPECT_EQ(4, folder_apps_grid_view()->cols());
  EXPECT_EQ(1, GetTotalPages(folder_apps_grid_view()));
  EXPECT_EQ(0, GetSelectedPage(folder_apps_grid_view()));
  EXPECT_TRUE(folder_apps_grid_view()->IsInFolder());
}

// There's no "page break" item at the end of first page with full grid.
TEST_P(AppsGridViewTabletTest, NoPageBreakItemWithFullGrid) {
  // There are two pages and last item is on second page.
  const int kApps = 2 + GetTilesPerPageInPagedGrid(0);
  GetTestModel()->PopulateApps(kApps);
  std::string model_content = "Item 0";
  for (int i = 1; i < kApps; ++i)
    model_content.append(",Item " + base::NumberToString(i));

  EXPECT_EQ(model_content, GetTestModel()->GetModelContent());
}

TEST_P(AppsGridViewClamshellAndTabletTest, RootGridUpdatesOnModelChange) {
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(2u, view_model->view_size());
  TestAppListItemViewIndice();

  // Update the model, and verify the apps grid gets updated.
  auto model_override = std::make_unique<test::AppListTestModel>();
  model_override->PopulateApps(3);
  model_override->CreateAndPopulateFolderWithApps(5);
  model_override->PopulateApps(3);

  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model = std::make_unique<QuickAppAccessModel>();

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model.get());
  UpdateLayout();

  // Verify that the view model size matches the new model.
  EXPECT_EQ(7u, view_model->view_size());
  TestAppListItemViewIndice();

  // Verify that clicking an item activates it.
  LeftClickOn(view_model->view_at(0));
  EXPECT_EQ("Item 0", GetTestAppListClient()->activate_item_last_id());

  // Clicking on the folder item transitions to folder view.
  LeftClickOn(view_model->view_at(3));
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());

  ASSERT_EQ(5u, folder_apps_grid_view()->view_model()->view_size());

  // Click on an item within the folder.
  LeftClickOn(folder_apps_grid_view()->view_model()->view_at(1));
  EXPECT_EQ("Item 4", GetTestAppListClient()->activate_item_last_id());

  // Switch model to original one, and verify the folder view gets closed.
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, GetTestModel(), GetAppListTestHelper()->search_model(),
      GetAppListTestHelper()->quick_app_access_model());
  UpdateLayout();
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_EQ(2u, view_model->view_size());
  TestAppListItemViewIndice();

  LeftClickOn(view_model->view_at(1));
  EXPECT_EQ("Item 1", GetTestAppListClient()->activate_item_last_id());

  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(0u, view_model->view_size());
}

TEST_P(AppsGridViewClamshellAndTabletTest,
       TouchScrollFromFolderNameDoesNotAffectRootGrid) {
  // Add enough items to the root grid so the launcher becomes paged.
  GetTestModel()->PopulateApps(1);
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // `TilesPerPage()` is not well defined for bubble launcher - populate bubble
  // launcher grid with arbitrary sufficiently large number of apps (so the
  // root grid becomes scrollable).
  GetTestModel()->PopulateApps(GetTilesPerPageOr(0, 30));
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
  GetTestModel()->PopulateApps(1);
  GetTestModel()->CreateAndPopulateFolderWithApps(5);
  // `GetTilesPerPageInPagedGrid()` may return a large number for bubble
  // launcher - ensure the number of test apps is not excessive.
  GetTestModel()->PopulateApps(GetTilesPerPageOr(0, 30));
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

TEST_P(AppsGridViewTabletTest, MoveItemToPreviousFullPage) {
  // There are two pages and last item is on second page.
  const size_t kApps = 2 + GetTilesPerPageInPagedGrid(0);
  GetTestModel()->PopulateApps(kApps);
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  GetPaginationModel()->SelectPage(1, false);
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 1, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(0, 0);
    gfx::Point to_in_previous_page =
        is_rtl_ ? tile_rect.right_center() : tile_rect.left_center();

    // Drag the last item to the first item's left position in previous
    // page.
    UpdateDragToNeighborPage(false /* next_page */, to_in_previous_page,
                             AppsGridView::MOUSE);
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  // The dragging is successful, the last item becomes the first item.
  EXPECT_EQ("0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(kApps, view_model->view_size());
  for (size_t i = 0; i < kApps; ++i) {
    int page = i / GetTilesPerPageInPagedGrid(0);
    int slot = i % GetTilesPerPageInPagedGrid(0);
    EXPECT_EQ(view_model->view_at(i),
              test_api_->GetViewAtVisualIndex(page, slot));
    EXPECT_EQ("Item " + base::NumberToString((i + kApps - 1) % kApps),
              view_model->view_at(i)->item()->id());
  }
  CheckHaptickEventsCount(1);
}

// Test that the background cards remain stacked as the bottom layer during
// an item drag. The adding of views to the apps grid during a drag (e.g. ghost
// image view) can cause a reorder of layers.
TEST_P(AppsGridViewTabletTest, BackgroundCardLayerOrderedAtBottom) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  // Start cardified apps grid.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 1, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
    EXPECT_EQ(nullptr, GetCurrentGhostImageView());

    test_api_->FireReorderTimerAndWaitForAnimationDone();
    // Check that the ghost image view was created.
    EXPECT_NE(nullptr, GetCurrentGhostImageView());

    ASSERT_EQ(1, paged_apps_grid_view_->BackgroundCardCountForTesting());

    // Check that the first background card layer is stacked at the bottom.
    EXPECT_EQ(paged_apps_grid_view_->GetBackgroundCardLayerForTesting(0),
              GetItemsContainer()->layer()->children()[0]);
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
}

TEST_P(AppsGridViewTabletTest, PeekingCardOnLastPage) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  // Start cardified apps grid.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
    EXPECT_EQ(1, paged_apps_grid_view_->BackgroundCardCountForTesting());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
}

TEST_P(AppsGridViewTabletTest, BackgroundCardBounds) {
  ASSERT_TRUE(paged_apps_grid_view_);
  GetTestModel()->PopulateApps(30);
  UpdateLayout();

  // Enter cardified state.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
    ASSERT_EQ(2, paged_apps_grid_view_->BackgroundCardCountForTesting());

    // Verify that all items in the current page fit within the background card.
    const gfx::Rect background_card_bounds =
        paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(0);
    const gfx::Rect clip_rect = paged_apps_grid_view_->GetMirroredRect(
        paged_apps_grid_view_->layer()->clip_rect());
    const gfx::Rect first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

    EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
        << " background card bounds " << background_card_bounds.ToString()
        << " item bounds " << first_item_bounds.ToString();
    EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
        << " clip rect " << clip_rect.ToString() << " item bounds "
        << first_item_bounds.ToString();

    const gfx::Rect last_item_bounds = GetItemRectOnCurrentPageAt(
        GetTilesPerPageInPagedGrid(0) / apps_grid_view_->cols() - 1,
        apps_grid_view_->cols() - 1);

    EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
        << " background card bounds " << background_card_bounds.ToString()
        << " item bounds " << last_item_bounds.ToString();
    EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
        << " clip rect " << clip_rect.ToString() << " item bounds "
        << last_item_bounds.ToString();
  }));

  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  EXPECT_EQ(gfx::Rect(), paged_apps_grid_view_->layer()->clip_rect());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(0, paged_apps_grid_view_->BackgroundCardCountForTesting());
}

TEST_P(AppsGridViewTabletTest, BackgroundCardBoundsOnSecondPage) {
  ASSERT_TRUE(paged_apps_grid_view_);
  GetTestModel()->PopulateApps(30);
  UpdateLayout();

  // Enter cardified state, and drag the item to the second apps grid page.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> drag_to_next_page;
  drag_to_next_page.push_back(base::BindLambdaForTesting([&]() {
    const gfx::Point to_in_next_page =
        test_api_->GetItemTileRectAtVisualIndex(1, 0).left_center();
    // Drag the first item to the next page to create another page.
    UpdateDragToNeighborPage(true /* next_page */, to_in_next_page,
                             AppsGridView::TOUCH);
  }));
  drag_to_next_page.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&drag_to_next_page, /*is_touch =*/true);

  ASSERT_EQ(1, GetPaginationModel()->selected_page());

  // Trigger cardified state again.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
    ASSERT_EQ(2, paged_apps_grid_view_->BackgroundCardCountForTesting());

    // Verify that all items in the current page fit within the background card.
    const gfx::Rect background_card_bounds =
        paged_apps_grid_view_->GetBackgroundCardBoundsForTesting(1);
    const gfx::Rect clip_rect = paged_apps_grid_view_->GetMirroredRect(
        paged_apps_grid_view_->layer()->clip_rect());
    const gfx::Rect first_item_bounds = GetItemRectOnCurrentPageAt(0, 0);

    EXPECT_TRUE(background_card_bounds.Contains(first_item_bounds))
        << " background card bounds " << background_card_bounds.ToString()
        << " item bounds " << first_item_bounds.ToString();
    EXPECT_TRUE(clip_rect.Contains(first_item_bounds))
        << " clip rect " << clip_rect.ToString() << " item bounds "
        << first_item_bounds.ToString();

    const gfx::Rect last_item_bounds = GetItemRectOnCurrentPageAt(
        GetTilesPerPageInPagedGrid(1) / apps_grid_view_->cols() - 1,
        apps_grid_view_->cols() - 1);

    EXPECT_TRUE(background_card_bounds.Contains(last_item_bounds))
        << " background card bounds " << background_card_bounds.ToString()
        << " item bounds " << last_item_bounds.ToString();
    EXPECT_TRUE(clip_rect.Contains(last_item_bounds))
        << " clip rect " << clip_rect.ToString() << " item bounds "
        << last_item_bounds.ToString();
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  EXPECT_EQ(gfx::Rect(), paged_apps_grid_view_->layer()->clip_rect());
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  EXPECT_EQ(0, paged_apps_grid_view_->BackgroundCardCountForTesting());
}

TEST_F(AppsGridViewTest, DragItemVisibleAfterDragInScrolledView) {
  const int kRootGridItems = 39;
  GetTestModel()->PopulateApps(kRootGridItems);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Start dragging the first item in the grid.
  StartDragForViewAndFireTimer(
      AppsGridView::MOUSE, GetItemViewInCurrentPageAt(0, 0, apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Scroll the bubble launcher apps grid so the last item is visible.
    test_api_->GetViewAtIndex(GridIndex(0, kRootGridItems - 1))
        ->ScrollViewToVisible();
    apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

    // Calculate the coordinates for the drop point - the drop point is the
    // first empty slot in the root apps grid.
    gfx::Point drop_point =
        test_api_->GetItemTileRectAtVisualIndex(0, kRootGridItems)
            .CenterPoint();

    UpdateDrag(AppsGridView::MOUSE, drop_point, apps_grid_view_, /*steps=*/5);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  EndDrag();
  CheckHaptickEventsCount(1);

  // Verify that the dragged item was dropped into the last slot in the grid,
  // and that it's within the visible apps grid bounds.
  AppListItemView* dropped_view =
      test_api_->GetViewAtIndex(GridIndex(0, kRootGridItems - 1));
  ASSERT_TRUE(dropped_view);
  EXPECT_EQ("Item 0", dropped_view->item()->id());
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      dropped_view->GetBoundsInScreen()));
}

TEST_F(AppsGridViewTest, DragItemVisibleAfterReparentDragInScrolledView) {
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  const int kRootGridItems = 41;
  GetTestModel()->PopulateApps(kRootGridItems - 1);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Open the folder view.
  test_api_->PressItemAt(0);

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_EQ("Item 0", drag_view->item()->id());
    CheckHaptickEventsCount(1);
    gfx::Point point_outside_folder =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height());
    UpdateDrag(AppsGridView::MOUSE, point_outside_folder,
               folder_apps_grid_view(), 10 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Scroll the bubble launcher apps grid so the last item is visible.
    test_api_->GetViewAtIndex(GridIndex(0, kRootGridItems - 1))
        ->ScrollViewToVisible();
    apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

    // Calculate the coordinates for the drop point - the drop point is the
    // first empty slot in the root apps grid.
    gfx::Point drop_point =
        GetItemRectOnCurrentPageAt(0, kRootGridItems).CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_, folder_apps_grid_view(),
                                      &drop_point);
    UpdateDrag(AppsGridView::MOUSE, drop_point, folder_apps_grid_view(),
               5 /*steps*/);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() { EndDrag(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);

  CheckHaptickEventsCount(1);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());

  // Verify that the dragged item was dropped into the last slot in the grid,
  // and that it's within the visible apps grid bounds.
  AppListItemView* dropped_view =
      test_api_->GetViewAtIndex(GridIndex(0, kRootGridItems));
  ASSERT_TRUE(dropped_view);
  EXPECT_EQ("Item 0", dropped_view->item()->id());
  EXPECT_TRUE(apps_grid_view_->GetWidget()->GetWindowBoundsInScreen().Contains(
      dropped_view->GetBoundsInScreen()));
}

TEST_P(AppsGridViewTabletTest, AppsGridIsCardifiedDuringDrag) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create only one page with two apps.
  GetTestModel()->PopulateApps(2);
  UpdateLayout();

  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);

    EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  CheckHaptickEventsCount(0);

  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
}

TEST_P(AppsGridViewTabletTest, DragWithinFolderDoesNotEnterCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Creates a folder item and open it.
  const size_t kTotalItems = kMaxItemsInFolder;
  GetTestModel()->CreateAndPopulateFolderWithApps(kTotalItems);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());

  // Drag the first folder child within the folder.
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view()));

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    const gfx::Point to =
        folder_grid_test_api.GetItemTileRectOnCurrentPageAt(0, 1).CenterPoint();
    UpdateDrag(AppsGridView::TOUCH, to, folder_apps_grid_view(), 10 /*steps*/);
    EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End the drag and check that no more item layer copies remain.
    EndDrag(AppsGridView::TOUCH);
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  CheckHaptickEventsCount(0);
}

TEST_P(AppsGridViewTabletTest, DragOutsideFolderEntersCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a folder item with some apps and open it.
  GetTestModel()->CreateAndPopulateFolderWithApps(3);
  test_api_->Update();
  test_api_->PressItemAt(0);
  AppsGridViewTestApi folder_grid_test_api(folder_apps_grid_view());

  // Drag the first folder child out of the folder.
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, folder_apps_grid_view());
  StartDragForViewAndFireTimer(AppsGridView::TOUCH, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    const gfx::Point to =
        app_list_folder_view()->GetLocalBounds().bottom_center() +
        gfx::Vector2d(0, drag_view->height()
                      /*padding to completely exit folder view*/);
    UpdateDrag(AppsGridView::TOUCH, to, folder_apps_grid_view(), 10 /*steps*/);
    EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  CheckHaptickEventsCount(0);
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
}

TEST_P(AppsGridViewTabletTest, DragItemIntoFolderStaysInCardifiedState) {
  ASSERT_TRUE(paged_apps_grid_view_);

  // Create a folder item with some apps. Add another app to the main grid.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  GetTestModel()->PopulateApps(1);
  UpdateLayout();
  StartDragForViewAndFireTimer(
      AppsGridView::TOUCH,
      GetItemViewInCurrentPageAt(0, 0, paged_apps_grid_view_));
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(0);
    // Dragging item_1 over folder to expand it.
    const gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
    UpdateDrag(AppsGridView::TOUCH, to, paged_apps_grid_view_, 10 /*steps*/);

    EXPECT_TRUE(paged_apps_grid_view_->cardified_state_for_testing());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { EndDrag(AppsGridView::TOUCH); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);

  CheckHaptickEventsCount(0);
  EXPECT_FALSE(paged_apps_grid_view_->cardified_state_for_testing());
  test_api_->WaitForItemMoveAnimationDone();
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewClamshellTest,
       ContextMenuInTopLevelAppListSortAllAppsInClamshellMode) {
  // In this test, the sort algorithm is not tested. Instead, the context menu
  // that contains the options to sort is verified to be shown in apps grid
  // view. The menu option selecting is also simulated to ensure the sorting is
  // called. The actual sort algorithm is tested in
  // chrome/browser/ash/app_list/app_list_sort_browsertest.cc.

  GetTestModel()->PopulateApps(1);

  AppsGridContextMenu* context_menu = apps_grid_view_->context_menu_for_test();
  EXPECT_FALSE(context_menu->IsMenuShowing());
  EXPECT_EQ(AppListSortOrder::kCustom, GetTestModel()->requested_sort_order());

  // Get a point in `apps_grid_view_` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_grid_view_->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Cache the current context menu view.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_EQ(reorder_option->title(), u"Name");

  // Open the Reorder by Name submenu.
  const gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            GetTestModel()->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Open the menu again to test the color sort option.
  SimulateRightClickOrLongPressAt(empty_space);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(reorder_option->title(), u"Color");

  const gfx::Point color_option =
      reorder_option->GetBoundsInScreen().CenterPoint();

  SimulateLeftClickOrTapAt(color_option);
  EXPECT_EQ(AppListSortOrder::kColor, GetTestModel()->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());
}

TEST_P(AppsGridViewTabletTest,
       ContextMenuInTopLevelAppListSortAllAppsInTabletMode) {
  GetTestModel()->PopulateApps(1);
  EXPECT_EQ(AppListSortOrder::kCustom, GetTestModel()->requested_sort_order());

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
  ASSERT_EQ(reorder_submenu->title(), u"Sort by");

  // Open the Sort by submenu.
  gfx::Point reorder_submenu_point =
      reorder_submenu->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_submenu_point);

  views::MenuItemView* reorder_option =
      reorder_submenu->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_EQ(reorder_option->title(), u"Name");
  gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);

  // Check that the apps are sorted and the menu is closed.
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            GetTestModel()->requested_sort_order());
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
  ASSERT_EQ(reorder_submenu->title(), u"Sort by");

  // Open the Sort by submenu.
  reorder_submenu_point = reorder_submenu->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_submenu_point);

  reorder_option = reorder_submenu->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_EQ(reorder_option->title(), u"Color");
  reorder_option_point = reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);

  // Check that the apps are sorted and the menu is closed.
  EXPECT_EQ(AppListSortOrder::kColor, GetTestModel()->requested_sort_order());
  EXPECT_EQ(
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing(),
      nullptr);
}

TEST_P(AppsGridViewTabletTest, NoSortOptionsWhenSearchPageIsShownInTabletMode) {
  GetTestModel()->PopulateApps(1);
  EXPECT_EQ(AppListSortOrder::kCustom, GetTestModel()->requested_sort_order());

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
  ASSERT_EQ(reorder_submenu->title(), u"Sort by");

  // Open the Sort by submenu.
  gfx::Point reorder_submenu_point =
      reorder_submenu->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_submenu_point);

  views::MenuItemView* reorder_option =
      reorder_submenu->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_EQ(reorder_option->title(), u"Name");
  gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);

  // Check that the apps are sorted and the menu is closed.
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            GetTestModel()->requested_sort_order());
  EXPECT_EQ(
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing(),
      nullptr);

  // Activate the search box.
  gfx::Point search_box_point =
      search_box_view_->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(search_box_point);

  // Open the menu again.
  SimulateRightClickOrLongPressAt(empty_space);
  context_menu =
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing();
  EXPECT_TRUE(context_menu->IsShowingMenu());

  // Verify that the sort option is removed and there are only 2 options in the
  // menu.
  int context_menu_size =
      context_menu->root_for_testing()->GetSubmenu()->GetMenuItems().size();
  EXPECT_LT(context_menu_size, 3);
}

TEST_P(AppsGridViewClamshellAndTabletTest, ContextMenuOnFolderItemSortAllApps) {
  // In this test, the sort algorithm is not tested. Instead, the context menu
  // that contains the options to sort is verified to be shown on folder app
  // list item view. The menu option selecting is also simulated to ensure the
  // sorting is called. The actual sort algorithm is tested in
  // chrome/browser/ash/app_list/app_list_sort_browsertest.cc.

  // Create a folder item and update the layout.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();
  EXPECT_EQ(AppListSortOrder::kCustom, GetTestModel()->requested_sort_order());

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
  ASSERT_EQ(reorder_option->title(), u"Name");

  // Open the Reorder by Name submenu.
  gfx::Point reorder_option_point =
      reorder_option->GetBoundsInScreen().CenterPoint();
  SimulateLeftClickOrTapAt(reorder_option_point);
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            GetTestModel()->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Open the menu again to test the color sort option.
  SimulateRightClickOrLongPressAt(folder_item_point);
  EXPECT_TRUE(context_menu->IsMenuShowing());

  reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(reorder_option->title(), u"Color");

  const gfx::Point color_option =
      reorder_option->GetBoundsInScreen().CenterPoint();

  SimulateLeftClickOrTapAt(color_option);
  EXPECT_EQ(AppListSortOrder::kColor, GetTestModel()->requested_sort_order());
  EXPECT_FALSE(context_menu->IsMenuShowing());
}

TEST_F(AppsGridViewTest, PulsingBlocksShowDuringAppListSync) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());

  // Set the model status as syncing. The Pulsing blocks model should not be
  // empty.
  GetTestModel()->SetStatus(AppListModelStatus::kStatusSyncing);
  UpdateLayout();
  EXPECT_NE(0u, GetPulsingBlocksModel().view_size());

  // Set the model status as normal. The Pulsing blocks model should be empty.
  GetTestModel()->SetStatus(AppListModelStatus::kStatusNormal);
  UpdateLayout();
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());
}

// Verify that pulsing blocks animation does not hang if animations are
// disabled.
TEST_P(AppsGridViewClamshellAndTabletTest,
       PulsingBlocksAnimationDoesNotHangWithDisabledAnimation) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Set the model status as syncing. The Pulsing blocks model should not be
  // empty.
  GetTestModel()->SetStatus(AppListModelStatus::kStatusSyncing);
  UpdateLayout();
  EXPECT_NE(0u, GetPulsingBlocksModel().view_size());

  PulsingBlockView* pulsing_block_view = GetPulsingBlocksModel().view_at(0);

  EXPECT_FALSE(pulsing_block_view->IsAnimating());
  EXPECT_TRUE(pulsing_block_view->FireAnimationTimerForTest());
  EXPECT_FALSE(pulsing_block_view->IsAnimating());

  // Test should not hang.
}

// Tests that the pulsing blocks animation runs.
TEST_P(AppsGridViewClamshellAndTabletTest,
       PulsingBlocksAnimationOnFiringAnimationTimer) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetTestModel()->SetStatus(AppListModelStatus::kStatusSyncing);
  UpdateLayout();
  if (GetParam()) {
    ASSERT_EQ(GetTilesPerPageInPagedGrid(0) - 3,
              GetPulsingBlocksModel().view_size());
  } else {
    ASSERT_EQ(static_cast<size_t>(apps_grid_view_->cols() +
                                  apps_grid_view_->cols() - 3),
              GetPulsingBlocksModel().view_size());
  }

  PulsingBlockView* pulsing_block_view = GetPulsingBlocksModel().view_at(0);

  EXPECT_FALSE(pulsing_block_view->IsAnimating());
  EXPECT_TRUE(pulsing_block_view->FireAnimationTimerForTest());
  EXPECT_TRUE(pulsing_block_view->IsAnimating());

  // Set the model status as normal to avoid the test hanging due to the
  // pulsing blocks animation.
  GetTestModel()->SetStatus(AppListModelStatus::kStatusNormal);
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());
}

// Verify that as new app items get synced into the app list, newer items slowly
// fade in place of a placeholder.
TEST_F(AppsGridViewTest, AppIconSubtitutesPulsingBlockView) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetTestModel()->SetStatus(AppListModelStatus::kStatusSyncing);
  UpdateLayout();

  const size_t initial_pulsing_blocks = GetPulsingBlocksModel().view_size();
  ASSERT_EQ(static_cast<size_t>(apps_grid_view_->cols() +
                                apps_grid_view_->cols() - 3),
            initial_pulsing_blocks);

  PulsingBlockView* pulsing_block_view = GetPulsingBlocksModel().view_at(0);

  EXPECT_TRUE(pulsing_block_view->FireAnimationTimerForTest());
  EXPECT_TRUE(pulsing_block_view->IsAnimating());

  gfx::Rect placeholder_bounds = pulsing_block_view->GetBoundsInScreen();

  // Add another app to simulate a synced app.
  GetTestModel()->PopulateApps(1);
  UpdateLayout();

  // The number of pulsing blocks will be decreased by one in order for the
  // incoming app to fade in its place.
  ASSERT_EQ(initial_pulsing_blocks - 1, GetPulsingBlocksModel().view_size());

  AppListItemView* item_view = GetItemViewInTopLevelGrid(3);

  ASSERT_TRUE(item_view->layer());
  EXPECT_TRUE(item_view->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, item_view->layer()->GetTargetOpacity());

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(item_view->layer());

  // The new item should be placed at the first placeholder bounds.
  EXPECT_EQ(placeholder_bounds, item_view->GetBoundsInScreen());

  // Set the model status as normal to avoid the test hanging due to the
  // pulsing blocks animation.
  GetTestModel()->SetStatus(AppListModelStatus::kStatusNormal);
  EXPECT_EQ(0u, GetPulsingBlocksModel().view_size());
}

// Tests that right clicking an app will remove focus from other apps within the
// apps grid. See https://crbug.com/1146365.
TEST_F(AppsGridViewTest, VerifyFocusRemovedWhenLeftClickingOtherApp) {
  GetTestModel()->PopulateApps(3);
  UpdateLayout();

  views::View* first_app = test_api_->GetViewAtModelIndex(0);
  views::View* last_app = test_api_->GetViewAtModelIndex(2);

  // Move focus to the last app
  last_app->RequestFocus();
  ASSERT_TRUE(last_app->HasFocus());

  SimulateRightClickOrLongPressAt(first_app->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(last_app->HasFocus());
}

// Tests that right clicking an app will remove focus from the title name of a
// folder. See https://crbug.com/1146365.
TEST_F(AppsGridViewTest, VerifyFocusRemovedWhenLeftClickingOtherAppForFolder) {
  // Create a folder with a couple items.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();

  // Open the folder item.
  test_api_->PressItemAt(0);
  ASSERT_TRUE(folder_apps_grid_view());

  // Force focus on the title name of the folder.
  app_list_folder_view()->FocusNameInput();
  ASSERT_TRUE(app_list_folder_view()->folder_header_view()->HasTextFocus());

  // Right click on another element.
  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(0, folder_apps_grid_view());
  SimulateRightClickOrLongPressAt(item_view->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(app_list_folder_view()->folder_header_view()->HasTextFocus());
}

// Tests that right clicking an app will remove focus from other apps within the
// apps grid. See https://crbug.com/1146365.
TEST_F(AppsGridViewTest, FocusNotRestoredIfNoViewWasFocused) {
  // Create a folder with a couple items.
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();

  // Open the folder item.
  test_api_->PressItemAt(0);
  ASSERT_TRUE(folder_apps_grid_view());

  // Force focus on the title name of the folder.
  app_list_folder_view()->FocusNameInput();
  ASSERT_TRUE(app_list_folder_view()->folder_header_view()->HasTextFocus());

  // Press Enter, the title should not have focus.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_RETURN);
  ASSERT_FALSE(app_list_folder_view()
                   ->folder_header_view()
                   ->IsFolderNameViewActiveForTest());

  // Right click on another element.
  AppListItemView* const item_view =
      GetItemViewInAppsGridAt(0, folder_apps_grid_view());
  SimulateRightClickOrLongPressAt(item_view->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(app_list_folder_view()->folder_header_view()->HasTextFocus());

  // Exit context menu. The focus should not get restored to the text.
  item_view->CancelContextMenu();
  EXPECT_FALSE(app_list_folder_view()->folder_header_view()->HasTextFocus());
}

TEST_P(AppsGridViewTabletTest, ChangeFolderNameShouldUpdateShadows) {
  SetVirtualKeyboardEnabled(true);

  const int kMaxAppsInGrid = test_api_->TilesPerPageInPagedGrid(0);
  GetTestModel()->PopulateApps(kMaxAppsInGrid - 1);
  UpdateLayout();

  // Create a folder on the second row with kMaxItemsInFolder to be big enough
  // to displace the apps grid bounds on keyboard shown. Open the folder.
  GetTestModel()->CreateAndPopulateFolderWithApps(kMaxItemsInFolder);
  test_api_->PressItemAt(kMaxAppsInGrid - 1);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  gfx::Rect initial_folder_bounds =
      folder_apps_grid_view()->GetBoundsInScreen();
  gfx::Rect initial_shadow =
      app_list_folder_view()->shadow()->GetContentBounds();
  views::View::ConvertRectToScreen(app_list_folder_view(), &initial_shadow);

  // Show the virtual keyboard. The grid view should displace with the folder
  // and shadow bounds.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect folder_bounds_with_keyboard =
      folder_apps_grid_view()->GetBoundsInScreen();
  gfx::Rect shadow_with_keyboard =
      app_list_folder_view()->shadow()->GetContentBounds();
  views::View::ConvertRectToScreen(app_list_folder_view(),
                                   &shadow_with_keyboard);

  EXPECT_NE(initial_folder_bounds, folder_bounds_with_keyboard);
  EXPECT_NE(initial_shadow, shadow_with_keyboard);

  // Start typing to change the folder name. The folder and shadow bounds must
  // stay unchanged from previous step.
  views::Textfield* folder_header =
      app_list_folder_view()->folder_header_view()->GetFolderNameViewForTest();
  folder_header->RequestFocus();
  ASSERT_TRUE(folder_header->HasFocus());
  const std::u16string folder_name = folder_header->GetText();

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
  // Force app list to Update Layout to catch potential crashes.
  UpdateLayout();

  EXPECT_NE(folder_name, folder_header->GetText());

  gfx::Rect folder_bounds_after_type =
      folder_apps_grid_view()->GetBoundsInScreen();
  gfx::Rect shadow_after_type =
      app_list_folder_view()->shadow()->GetContentBounds();
  views::View::ConvertRectToScreen(app_list_folder_view(), &shadow_after_type);

  EXPECT_EQ(folder_bounds_with_keyboard, folder_bounds_after_type);
  EXPECT_EQ(shadow_with_keyboard, shadow_after_type);
}

// Test that root level item animations run correctly after quickly dragging and
// dropping an item from a folder into the root level grid.
TEST_P(AppsGridViewClamshellAndTabletTest, QuickDragToRemoveItemFromFolder) {
  GetTestModel()->PopulateApps(9);
  GetTestModel()->CreateAndPopulateFolderWithApps(2);
  UpdateLayout();

  AppListItemView* folder_item_view = GetItemViewInTopLevelGrid(9);

  // Open the folder.
  EXPECT_TRUE(folder_item_view->is_folder());
  SimulateLeftClickOrTapAt(folder_item_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_EQ(2u, folder_apps_grid_view()->view_model()->view_size());

  AppListItemView* item_in_folder = folder_apps_grid_view()->GetItemViewAt(0);

  // Enable animation times once the folder is open, to test animations when
  // dragging items out.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  StartDragForViewAndFireTimer(AppsGridView::MOUSE, item_in_folder);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag item outside of the folder, to slot 0.
    const gfx::Point to = apps_grid_view_->GetItemViewAt(0)
                              ->GetIconBoundsInScreen()
                              .left_center();
    UpdateDrag(AppsGridView::MOUSE, to, apps_grid_view_, 10 /*steps*/);

    ASSERT_TRUE(folder_apps_grid_view()->has_dragged_item());
    ASSERT_TRUE(folder_apps_grid_view()->IsDragging());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Release drag.
    EndDrag(AppsGridView::MOUSE);
    ASSERT_FALSE(GetAppListTestHelper()->IsInFolderView());
    EXPECT_EQ(folder_item_view->item()->ChildItemCount(), 1u);

    apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

    // After releasing drag, check that the root item views are still animating.
    for (size_t i = 1; i < apps_grid_view_->view_model()->view_size(); i++) {
      auto* item_view = apps_grid_view_->view_model()->view_at(i);
      EXPECT_TRUE((item_view->layer()->GetAnimator()->is_animating() ||
                   apps_grid_view_->IsAnimatingView(item_view)));
    }
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/true);
}

// TODO(crbug.com/1371184): Fix flaky test.
TEST_P(AppsGridViewClamshellAndTabletTest,
       DISABLED_ReorderDragAnimationMetrics) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histogram_tester;
  const int kAppsInGrid = 9;
  GetTestModel()->PopulateApps(kAppsInGrid);
  UpdateLayout();

  // Begin item drag.
  auto* dragged_item = apps_grid_view_->GetItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(
      dragged_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  dragged_item->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Wait for layer animations before the drag to trigger drag reorder
  // animations.
  for (size_t i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    auto* view = apps_grid_view_->view_model()->view_at(i);
    if (view->layer())
      ui::LayerAnimationStoppedWaiter().Wait(view->layer());
  }

  ASSERT_TRUE(apps_grid_view_->drag_item());
  ASSERT_TRUE(apps_grid_view_->IsDragging());
  ASSERT_EQ(dragged_item->item(), apps_grid_view_->drag_item());

  // Drag item to the last slot
  const gfx::Point drop_point = apps_grid_view_->GetItemViewAt(kAppsInGrid - 1)
                                    ->GetIconBoundsInScreen()
                                    .right_center() +
                                gfx::Vector2d(100, 10);
  GetEventGenerator()->MoveMouseTo(drop_point);
  ASSERT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
  apps_grid_view_->reorder_timer_for_test()->FireNow();

  // Let the animations play out.
  test_api_->WaitForItemMoveAnimationDone();

  // Release drag.
  GetEventGenerator()->ReleaseLeftButton();

  // Ensure there is one more frame presented after animation finishes to allow
  // animation throughput data to be passed from cc to ui.
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(
      apps_grid_view_->GetWidget()->GetCompositor()));

  if (create_as_tablet_mode_) {
    histogram_tester.ExpectTotalCount(
        kTabletDragReorderAnimationSmoothnessHistogram, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        kClamshellDragReorderAnimationSmoothnessHistogram, 1);
  }
}

TEST_F(AppsGridViewTest, PromiseIconLayers) {
  AppListItem* item = GetTestModel()->CreateAndAddPromiseItem("PromiseApp");
  const std::string promise_app_id = item->GetMetadata()->id;
  UpdateLayout();

  AppListItemView* promise_view = apps_grid_view_->GetItemViewAt(0);

  // Promise apps are created with app_status kPending.
  EXPECT_EQ(promise_view->item()->progress(), -1.0f);
  EXPECT_TRUE(promise_view->layer());

  // Change app status to installing and send a progress update.
  item->UpdateAppStatusForTesting(AppStatus::kInstalling);
  item->SetProgress(0.3f);
  EXPECT_EQ(promise_view->item()->progress(), 0.3f);
  EXPECT_TRUE(promise_view->layer());

  // Set the last status update to kInstallSuccess as if the app had finished
  // installing.
  item->UpdateAppStatusForTesting(AppStatus::kInstallSuccess);
  EXPECT_TRUE(promise_view->layer());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate pushing the installed app.
  GetTestModel()->DeleteItem(item->id());

  EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));

  auto* installed_item = GetTestModel()->CreateItem("installed_id");
  auto installed_item_metadata = installed_item->CloneMetadata();
  installed_item_metadata->promise_package_id = promise_app_id;
  installed_item->SetMetadata(std::move(installed_item_metadata));
  GetTestModel()->AddItem(std::move(installed_item));

  AppListItemView* installed_view = apps_grid_view_->GetItemViewAt(0);
  EXPECT_EQ(installed_view->item()->id(), "installed_id");
  ASSERT_TRUE(installed_view->layer());
  EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));

  // Verify that the layer is still animating.
  ASSERT_TRUE(installed_view->GetIconView()->layer());
  EXPECT_TRUE(
      installed_view->GetIconView()->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(installed_view->GetIconView()->layer());

  EXPECT_FALSE(installed_view->GetIconView()->layer());
  EXPECT_FALSE(HasPendingPromiseAppRemoval(promise_app_id));
  EXPECT_FALSE(installed_view->layer());
}

TEST_F(AppsGridViewTest, PromiseAppsSharePackage) {
  AppListItem* first_item =
      GetTestModel()->CreateAndAddPromiseItem("PromiseApp");
  const std::string promise_app_id = first_item->GetMetadata()->id;
  UpdateLayout();

  AppListItemView* first_promise_view = apps_grid_view_->GetItemViewAt(0);

  // Promise apps are created with app_status kPending.
  EXPECT_EQ(first_promise_view->item()->progress(), -1.0f);
  EXPECT_TRUE(first_promise_view->layer());

  // Set the last status update to kInstallSuccess as if the app had finished
  // installing.
  first_item->UpdateAppStatusForTesting(AppStatus::kInstallSuccess);
  EXPECT_TRUE(first_promise_view->layer());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate pushing the installed app.
  GetTestModel()->DeleteItem(first_item->id());
  EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));
  auto* first_installed_item = GetTestModel()->CreateItem("installed_id1");
  auto item_metadata = first_installed_item->CloneMetadata();
  item_metadata->promise_package_id = promise_app_id;
  first_installed_item->SetMetadata(std::move(item_metadata));
  GetTestModel()->AddItem(std::move(first_installed_item));

  // While the first item is waiting for the installed item, create a new
  // promise app with the same package id and immediately push it.
  EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));
  AppListItem* new_item = GetTestModel()->CreateAndAddPromiseItem("PromiseApp");
  EXPECT_EQ(promise_app_id, new_item->id());
  EXPECT_EQ(2u, apps_grid_view_->view_model()->view_size());
  AppListItemView* new_promise_view = apps_grid_view_->GetItemViewAt(1);
  EXPECT_EQ(promise_app_id, new_promise_view->item()->id());
  GetTestModel()->DeleteItem(promise_app_id);
  EXPECT_EQ(1u, apps_grid_view_->view_model()->view_size());

  auto* new_installed_item = GetTestModel()->CreateItem("installed_id2");
  item_metadata = new_installed_item->CloneMetadata();
  item_metadata->promise_package_id = promise_app_id;
  new_installed_item->SetMetadata(std::move(item_metadata));
  GetTestModel()->AddItem(std::move(new_installed_item));
  EXPECT_EQ(2u, apps_grid_view_->view_model()->view_size());

  AppListItemView* first_installed_view = apps_grid_view_->GetItemViewAt(0);
  EXPECT_EQ(first_installed_view->item()->id(), "installed_id1");
  ASSERT_TRUE(first_installed_view->layer());
  AppListItemView* new_installed_view = apps_grid_view_->GetItemViewAt(1);
  EXPECT_EQ(new_installed_view->item()->id(), "installed_id2");
  ASSERT_TRUE(new_installed_view->layer());
  EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));

  // Verify that the layers are animating separately.
  ASSERT_TRUE(first_installed_view->GetIconView()->layer());
  EXPECT_TRUE(first_installed_view->GetIconView()
                  ->layer()
                  ->GetAnimator()
                  ->is_animating());
  ASSERT_TRUE(new_installed_view->GetIconView()->layer());
  EXPECT_TRUE(new_installed_view->GetIconView()
                  ->layer()
                  ->GetAnimator()
                  ->is_animating());

  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(first_installed_view->GetIconView()->layer());

  // Both layer animations should end at about the same time, however, if the
  // animation for the other view isnot over, wait for it.
  if (new_installed_view->layer()) {
    ui::LayerAnimationStoppedWaiter new_animation_waiter;
    new_animation_waiter.Wait(new_installed_view->GetIconView()->layer());
  }

  EXPECT_FALSE(HasPendingPromiseAppRemoval(promise_app_id));
  EXPECT_FALSE(first_installed_view->GetIconView()->layer());
  EXPECT_FALSE(first_installed_view->layer());
  EXPECT_FALSE(new_installed_view->GetIconView()->layer());
  EXPECT_FALSE(new_installed_view->layer());
}

TEST_F(AppsGridViewTest, DragEndsDuringPromiseAppReplacement) {
  GetTestModel()->PopulateApps(1);
  AppListItem* item = GetTestModel()->CreateAndAddPromiseItem("PromiseApp");
  const std::string promise_app_id = item->GetMetadata()->id;
  UpdateLayout();

  AppListItemView* promise_view = apps_grid_view_->GetItemViewAt(1);

  // Promise apps are created with app_status kPending.
  EXPECT_EQ(promise_view->item()->progress(), -1.0f);
  EXPECT_TRUE(promise_view->layer());

  // Change app status to installing and send a progress update.
  item->UpdateAppStatusForTesting(AppStatus::kInstalling);
  item->SetProgress(0.3f);
  EXPECT_EQ(promise_view->item()->progress(), 0.3f);
  EXPECT_TRUE(promise_view->layer());

  // Set the last status update to kInstallSuccess as if the app had finished
  // installing.
  item->UpdateAppStatusForTesting(AppStatus::kInstallSuccess);
  EXPECT_TRUE(promise_view->layer());

  AppListItemView* const dragged_item_view =
      GetItemViewInCurrentPageAt(0, 0, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, dragged_item_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(apps_grid_view_->drag_item());
    ASSERT_TRUE(apps_grid_view_->IsDragging());
    ASSERT_EQ(dragged_item_view->item(), apps_grid_view_->drag_item());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Simulate promise item getting replaced.
    {
      ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
          ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

      // Simulate pushing the installed app.
      GetTestModel()->DeleteItem(item->id());

      EXPECT_TRUE(HasPendingPromiseAppRemoval(promise_app_id));

      auto* installed_item = GetTestModel()->CreateItem("installed_id");
      auto installed_item_metadata = installed_item->CloneMetadata();
      installed_item_metadata->promise_package_id = promise_app_id;
      installed_item->SetMetadata(std::move(installed_item_metadata));
      GetTestModel()->AddItem(std::move(installed_item));
    }

    // End drag while the promise app replacement animation is still in
    // progress.
    EndDrag();
  }));

  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  AppListItemView* installed_view = apps_grid_view_->GetItemViewAt(1);
  ASSERT_TRUE(installed_view);
  EXPECT_EQ("installed_id", installed_view->item()->id());
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(installed_view->GetIconView()->layer());

  // Make sure the drop animation completed before checking whether the
  // installed view has a layer (item view layers are present until the drop
  // animation completes).
  ui::Layer* drag_icon_layer = GetDragIconLayer(apps_grid_view_);
  if (drag_icon_layer) {
    ui::LayerAnimationStoppedWaiter drop_animation_waiter;
    drop_animation_waiter.Wait(drag_icon_layer);
  }

  EXPECT_FALSE(installed_view->GetIconView()->layer());
  EXPECT_FALSE(HasPendingPromiseAppRemoval(promise_app_id));
  EXPECT_FALSE(installed_view->layer());
}

TEST_P(AppsGridViewDragTest, DraggedItemExitsGridItemExitsDragState) {
  size_t kTotalItems = 2;
  GetTestModel()->PopulateApps(kTotalItems);
  UpdateLayout();
  AppListItemView* drag_view =
      GetItemViewInCurrentPageAt(0, 0, apps_grid_view_);
  StartDragForViewAndFireTimer(AppsGridView::MOUSE, drag_view);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    CheckHaptickEventsCount(1);

    // Move item outside of the grid.
    UpdateDragInScreen(AppsGridView::MOUSE,
                       apps_grid_view_->GetBoundsInScreen().top_center() +
                           gfx::Vector2d(0, -drag_view->height()
                                         /*padding to completely exit view*/),
                       /*steps=*/10);
  }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { EXPECT_FALSE(IsUIStateDraggingForItemView(drag_view)); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Needed by the controller
    EndDrag();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch =*/false);
}

}  // namespace test
}  // namespace ash
