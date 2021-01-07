// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_folder_delegate.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace test {

namespace {

constexpr int kNumOfSuggestedApps = 3;

class PageFlipWaiter : public ash::PaginationModelObserver {
 public:
  explicit PageFlipWaiter(ash::PaginationModel* model) : model_(model) {
    model_->AddObserver(this);
  }

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
  ash::PaginationModel* model_ = nullptr;
  bool wait_ = false;
  std::string selected_pages_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipWaiter);
};

// WindowDeletionWaiter waits for the specified window to be deleted.
class WindowDeletionWaiter : aura::WindowObserver {
 public:
  explicit WindowDeletionWaiter(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
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

  DISALLOW_COPY_AND_ASSIGN(WindowDeletionWaiter);
};

// Find the window with type WINDOW_TYPE_MENU and returns the firstly found one.
// Returns nullptr if no such window exists.
aura::Window* FindMenuWindow(aura::Window* root) {
  if (root->type() == aura::client::WINDOW_TYPE_MENU)
    return root;
  for (auto* child : root->children()) {
    auto* menu_in_child = FindMenuWindow(child);
    if (menu_in_child)
      return menu_in_child;
  }
  return nullptr;
}

// Dragging task to be run after page flip is observed.
class DragAfterPageFlipTask : public ash::PaginationModelObserver {
 public:
  DragAfterPageFlipTask(ash::PaginationModel* model,
                        AppsGridView* view,
                        const ui::MouseEvent& drag_event)
      : model_(model), view_(view), drag_event_(drag_event) {
    model_->AddObserver(this);
  }

  ~DragAfterPageFlipTask() override { model_->RemoveObserver(this); }

 private:
  // PaginationModelObserver overrides:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override {
  }
  void SelectedPageChanged(int old_selected, int new_selected) override {
    view_->UpdateDragFromItem(AppsGridView::MOUSE, drag_event_);
  }
  void TransitionStarted() override {}
  void TransitionChanged() override {}
  void TransitionEnded() override {}

  ash::PaginationModel* model_;
  AppsGridView* view_;
  ui::MouseEvent drag_event_;

  DISALLOW_COPY_AND_ASSIGN(DragAfterPageFlipTask);
};

class TestSuggestedSearchResult : public TestSearchResult {
 public:
  TestSuggestedSearchResult() {
    set_display_type(ash::SearchResultDisplayType::kChip);
    set_is_recommendation(true);
  }
  ~TestSuggestedSearchResult() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSuggestedSearchResult);
};

}  // namespace

class AppsGridViewTest : public views::ViewsTestBase,
                         public testing::WithParamInterface<bool> {
 public:
  AppsGridViewTest() = default;
  explicit AppsGridViewTest(bool create_as_tablet_mode)
      : create_as_tablet_mode_(create_as_tablet_mode) {}
  ~AppsGridViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      is_rtl_ = GetParam();
      if (is_rtl_)
        base::i18n::SetICUDefaultLocale("he");
    }
    views::ViewsTestBase::SetUp();
    gfx::NativeView parent = GetContext();
    // Ensure that parent is big enough to show the full AppListView.
    parent->SetBounds(gfx::Rect(gfx::Point(0, 0), gfx::Size(1024, 768)));
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    delegate_->SetIsTabletModeEnabled(create_as_tablet_mode_);
    app_list_view_ = new AppListView(delegate_.get());
    app_list_view_->InitView(parent);
    app_list_view_->Show(AppListViewState::kFullscreenAllApps,
                         false /*is_side_shelf*/);
    contents_view_ = app_list_view_->app_list_main_view()->contents_view();
    apps_grid_view_ = contents_view_->apps_container_view()->apps_grid_view();
    app_list_view_->GetWidget()->Show();

    model_ = delegate_->GetTestModel();
    search_model_ = delegate_->GetSearchModel();
    suggestions_container_ = contents_view_->apps_container_view()
                                 ->suggestion_chip_container_view_for_test();
    expand_arrow_view_ = contents_view_->expand_arrow_view();
    for (size_t i = 0; i < kNumOfSuggestedApps; ++i) {
      search_model_->results()->Add(
          std::make_unique<TestSuggestedSearchResult>());
    }
    // Needed to update suggestions from |model_|.
    suggestions_container_->Update();

    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view_);
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }
  void TearDown() override {
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    app_list_view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

 protected:
  AppListItemView* GetItemViewAt(int index) const {
    return static_cast<AppListItemView*>(test_api_->GetViewAtModelIndex(index));
  }

  AppListItemView* GetItemViewForPoint(const gfx::Point& point) const {
    for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i) {
      AppListItemView* view = GetItemViewAt(i);
      gfx::Point view_origin = view->origin();
      views::View::ConvertPointToTarget(view->parent(), apps_grid_view_,
                                        &view_origin);
      if (gfx::Rect(view_origin, view->size()).Contains(point))
        return view;
    }
    return nullptr;
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(model_->top_level_item_list()->item_count(), 0u);
    return test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  int GetTilesPerPage(int page) const { return test_api_->TilesPerPage(page); }

  ash::PaginationModel* GetPaginationModel() const {
    return apps_grid_view_->pagination_model();
  }

  AppListFolderView* app_list_folder_view() const {
    return contents_view_->apps_container_view()->app_list_folder_view();
  }

  // Points are in |apps_grid_view_|'s coordinates, and fixed for RTL.
  AppListItemView* SimulateDrag(AppsGridView::Pointer pointer,
                                const gfx::Point& from,
                                const gfx::Point& to) {
    AppListItemView* view = GetItemViewForPoint(from);
    DCHECK(view);

    gfx::NativeWindow window = app_list_view_->GetWidget()->GetNativeWindow();
    gfx::Point root_from(from);
    views::View::ConvertPointToWidget(apps_grid_view_, &root_from);
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_from);
    // Ensure that the |root_from| point is correct if RTL.
    root_from.set_x(apps_grid_view_->GetMirroredXInView(root_from.x()));
    gfx::Point root_to(to);
    views::View::ConvertPointToWidget(apps_grid_view_, &root_to);
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_to);
    // Ensure that the |root_to| point is correct if RTL.
    root_to.set_x(apps_grid_view_->GetMirroredXInView(root_to.x()));
    apps_grid_view_->InitiateDrag(view, pointer, from, root_from);

    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, to, root_to,
                              ui::EventTimeForNow(), 0, 0);
    apps_grid_view_->UpdateDragFromItem(pointer, drag_event);
    return view;
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
    ui::GestureEvent gesture_event(
        apps_grid_view_->GetMirroredXInView(location.x()), location.y(), 0,
        base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    apps_grid_view_->OnGestureEvent(&gesture_event);
    return gesture_event;
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

  gfx::Point GetDragViewCenter() {
    gfx::Point drag_view_center =
        apps_grid_view_->drag_view()->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(apps_grid_view_->drag_view(),
                                      apps_grid_view_, &drag_view_center);
    return drag_view_center;
  }

  AppListView* app_list_view_ = nullptr;    // Owned by native widget.
  AppsGridView* apps_grid_view_ = nullptr;  // Owned by |app_list_view_|.
  ContentsView* contents_view_ = nullptr;   // Owned by |app_list_view_|.
  SearchResultContainerView* suggestions_container_ =
      nullptr;                                    // Owned by |apps_grid_view_|.
  ExpandArrowView* expand_arrow_view_ = nullptr;  // Owned by |apps_grid_view_|.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  AppListTestModel* model_ = nullptr;    // Owned by |delegate_|.
  SearchModel* search_model_ = nullptr;  // Owned by |delegate_|.
  std::unique_ptr<AppsGridViewTestApi> test_api_;
  bool is_rtl_ = false;
  bool test_with_fullscreen_ = true;
  bool create_as_tablet_mode_ = false;

 private:
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardUIController keyboard_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTest);
};

INSTANTIATE_TEST_SUITE_P(All, AppsGridViewTest, testing::Bool());

class TestAppsGridViewFolderDelegate : public AppsGridViewFolderDelegate {
 public:
  TestAppsGridViewFolderDelegate() = default;
  ~TestAppsGridViewFolderDelegate() override = default;

  void ReparentItem(AppListItemView* original_drag_view,
                    const gfx::Point& drag_point_in_folder_grid,
                    bool has_native_drag) override {}

  void DispatchDragEventForReparent(
      AppsGridView::Pointer pointer,
      const gfx::Point& drag_point_in_folder_grid) override {}

  void DispatchEndDragEventForReparent(bool events_forwarded_to_drag_drop_host,
                                       bool cancel_drag) override {}

  bool IsPointOutsideOfFolderBoundary(const gfx::Point& point) override {
    return false;
  }

  bool IsOEMFolder() const override { return false; }

  void SetRootLevelDragViewVisible(bool visible) override {}

  void HandleKeyboardReparent(AppListItemView* reparented_item,
                              ui::KeyboardCode key_code) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppsGridViewFolderDelegate);
};

TEST_P(AppsGridViewTest, CreatePage) {
  // Fully populates a page.
  const int kPages = 1;

  EXPECT_EQ(kNumOfSuggestedApps, suggestions_container_->num_results());
  // For new style launcher, each page has the same number of rows.
  const int kExpectedTilesOnFirstPage =
      apps_grid_view_->cols() * (apps_grid_view_->rows_per_page());
  EXPECT_EQ(kExpectedTilesOnFirstPage, GetTilesPerPage(kPages - 1));

  model_->PopulateApps(kPages * GetTilesPerPage(kPages - 1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());

  // Adds one more and gets a new page created.
  model_->CreateAndAddItem("Extra");
  EXPECT_EQ(kPages + 1, GetPaginationModel()->total_pages());
}

TEST_F(AppsGridViewTest, RemoveSelectedLastApp) {
  const int kTotalItems = 2;
  const int kLastItemIndex = kTotalItems - 1;

  model_->PopulateApps(kTotalItems);

  AppListItemView* last_view = GetItemViewAt(kLastItemIndex);
  apps_grid_view_->SetSelectedView(last_view);
  model_->DeleteItem(model_->GetItemName(kLastItemIndex));

  EXPECT_FALSE(apps_grid_view_->IsSelectedView(last_view));

  // No crash happens.
  AppListItemView* view = GetItemViewAt(0);
  apps_grid_view_->SetSelectedView(view);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(view));
}

// Tests that UMA is properly collected when either a suggested or normal app is
// launched.
TEST_F(AppsGridViewTest, UMATestForLaunchingApps) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(5);

  // Select the first app in grid and launch it.
  contents_view_->GetAppListMainView()->ActivateApp(GetItemViewAt(0)->item(),
                                                    0);

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
TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseCrash) {
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  model_->PopulateApps(cols * 2);

  AppListItemView* view0 = GetItemViewAt(0);
  model_->top_level_item_list()->MoveItem(0, cols + 2);

  // Make sure the logical location of the view.
  EXPECT_NE(view0, GetItemViewAt(0));
  EXPECT_EQ(view0, GetItemViewAt(cols + 2));

  // |view0| should be animating with layer.
  EXPECT_TRUE(view0->layer());

  test_api_->WaitForItemMoveAnimationDone();
  // |view0| layer should be cleared after the animation.
  EXPECT_FALSE(view0->layer());
  EXPECT_EQ(view0->bounds(), GetItemRectOnCurrentPageAt(1, 2));
}

TEST_F(AppsGridViewTest, MoveItemAcrossRowDoesNotCauseAnimation) {
  const int cols = apps_grid_view_->cols();
  ASSERT_LE(0, cols);
  model_->PopulateApps(cols * 2);

  apps_grid_view_->GetWidget()->Hide();

  AppListItemView* view0 = GetItemViewAt(0);
  model_->top_level_item_list()->MoveItem(0, cols + 2);

  // Make sure the logical location of the view.
  EXPECT_NE(view0, GetItemViewAt(0));
  EXPECT_EQ(view0, GetItemViewAt(cols + 2));

  // The item should be repositioned immediately when the widget is not visible.
  EXPECT_FALSE(view0->layer());
  EXPECT_EQ(view0->bounds(), GetItemRectOnCurrentPageAt(1, 2));
}

// Tests that control + arrow while a suggested chip is focused does not crash.
TEST_F(AppsGridViewTest, ControlArrowOnSuggestedChip) {
  model_->PopulateApps(5);
  suggestions_container_->children().front()->RequestFocus();

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(suggestions_container_->children().front(),
            apps_grid_view_->GetFocusManager()->GetFocusedView());
}

TEST_F(AppsGridViewTest, ItemLabelShortNameOverride) {
  // If the app's full name and short name differ, the title label's tooltip
  // should always be the full name of the app.
  std::string expected_text("xyz");
  std::string expected_tooltip("tooltip");
  AppListItem* item = model_->CreateAndAddItem("Item with short name");
  model_->SetItemNameAndShortName(item, expected_tooltip, expected_text);

  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_EQ(base::ASCIIToUTF16(expected_tooltip),
            item_view->GetTooltipText(title_label->bounds().CenterPoint()));
  EXPECT_EQ(base::ASCIIToUTF16(expected_text), title_label->GetText());
}

TEST_F(AppsGridViewTest, ItemLabelNoShortName) {
  // If the app's full name and short name are the same, use the default tooltip
  // behavior of the label (only show a tooltip if the title is truncated).
  std::string title("a");
  AppListItem* item = model_->CreateAndAddItem(title);
  model_->SetItemNameAndShortName(item, title, "");

  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_TRUE(
      title_label->GetTooltipText(title_label->bounds().CenterPoint()).empty());
  EXPECT_EQ(base::ASCIIToUTF16(title), title_label->GetText());
}

TEST_P(AppsGridViewTest, ScrollSequenceHandledByAppListView) {
  base::HistogramTester histogram_tester;

  model_->PopulateApps(GetTilesPerPage(0) + 1);
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

TEST_F(AppsGridViewTest,
       OnGestureEventScrollSequenceHandleByPaginationController) {
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

TEST_F(AppsGridViewTest, CloseFolderByClickingBackground) {
  AppsContainerView* apps_container_view =
      contents_view_->apps_container_view();

  const size_t kTotalItems =
      AppListConfig::instance().max_folder_items_per_page();
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(apps_container_view->IsInFolderView());

  // Simulate mouse press on folder background to close the folder.
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  apps_container_view->folder_background_view()->OnMouseEvent(&event);
  EXPECT_FALSE(apps_container_view->IsInFolderView());
}

// Tests that taps between apps within the AppsGridView does not result in the
// AppList closing.
TEST_P(AppsGridViewTest, TapsBetweenAppsWontCloseAppList) {
  model_->PopulateApps(2);
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

TEST_F(AppsGridViewTest, PageResetAfterOpenFolder) {
  const size_t kTotalItems =
      AppListConfig::instance().max_folder_pages() *
      AppListConfig::instance().max_folder_items_per_page();
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder. It should be at page 0.
  test_api_->PressItemAt(0);
  ash::PaginationModel* pagination_model =
      app_list_folder_view()->items_grid_view()->pagination_model();
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

TEST_F(AppsGridViewTest, FolderColsAndRows) {
  // Populate folders with different number of apps.
  model_->CreateAndPopulateFolderWithApps(2);
  model_->CreateAndPopulateFolderWithApps(5);
  model_->CreateAndPopulateFolderWithApps(9);
  model_->CreateAndPopulateFolderWithApps(15);
  model_->CreateAndPopulateFolderWithApps(17);

  // Check the number of cols and rows for each opened folder.
  AppsGridView* items_grid_view = app_list_folder_view()->items_grid_view();
  test_api_->PressItemAt(0);
  EXPECT_EQ(2, items_grid_view->view_model()->view_size());
  EXPECT_EQ(2, items_grid_view->cols());
  EXPECT_EQ(1, items_grid_view->rows_per_page());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(1);
  EXPECT_EQ(5, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  EXPECT_EQ(2, items_grid_view->rows_per_page());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(2);
  EXPECT_EQ(9, items_grid_view->view_model()->view_size());
  EXPECT_EQ(3, items_grid_view->cols());
  EXPECT_EQ(3, items_grid_view->rows_per_page());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(3);
  EXPECT_EQ(15, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  EXPECT_EQ(4, items_grid_view->rows_per_page());
  app_list_folder_view()->CloseFolderPage();

  test_api_->PressItemAt(4);
  EXPECT_EQ(17, items_grid_view->view_model()->view_size());
  EXPECT_EQ(4, items_grid_view->cols());
  EXPECT_EQ(4, items_grid_view->rows_per_page());
  app_list_folder_view()->CloseFolderPage();
}

TEST_P(AppsGridViewTest, ScrollDownShouldNotExitFolder) {
  const size_t kTotalItems =
      AppListConfig::instance().max_folder_items_per_page();
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(contents_view_->apps_container_view()->IsInFolderView());

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
  EXPECT_TRUE(contents_view_->apps_container_view()->IsInFolderView());
}

// Tests that an app icon is selected when a menu is shown by click.
TEST_F(AppsGridViewTest, AppIconSelectedWhenMenuIsShown) {
  model_->PopulateApps(1);
  ASSERT_EQ(1u, model_->top_level_item_list()->item_count());
  AppListItemView* app = GetItemViewAt(0);
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(app));

  // Send a mouse event which would show a context menu.
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_RIGHT_MOUSE_BUTTON,
                             ui::EF_RIGHT_MOUSE_BUTTON);

  // Use a views::View* to expose OnMouseEvent.
  views::View* const view = app;
  view->OnMouseEvent(&press_event);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(app));

  ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  view->OnMouseEvent(&release_event);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(app));

  // Cancel the menu, |app| should no longer be selected.
  app->CancelContextMenu();
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(app));
}

// Tests that the context menu for app item appears at the right position.
TEST_P(AppsGridViewTest, MenuAtRightPosition) {
  const size_t kItemsInPage =
      apps_grid_view_->cols() * apps_grid_view_->rows_per_page();
  const size_t kPages = 2;
  model_->PopulateApps(kItemsInPage * kPages);

  auto* root = apps_grid_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  gfx::Rect root_bounds = root->GetBoundsInScreen();

  std::vector<int> pages_to_check = {1, 0};
  for (int i : pages_to_check) {
    apps_grid_view_->pagination_model()->SelectPage(i, /*animate=*/false);

    for (size_t j = 0; j < kItemsInPage; ++j) {
      const size_t idx = kItemsInPage * i + j;
      AppListItemView* item_view = GetItemViewAt(idx);

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

TEST_P(AppsGridViewTest, MouseDragItemIntoFolder) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  // Normally individual item-view does not have a layer.
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_FALSE(GetItemViewAt(i)->layer());
  EXPECT_EQ(model_->top_level_item_list()->item_count(), kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2"), model_->GetModelContent());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();

  // Dragging item_1 over item_0 creates a folder.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  // Each item view has its own layer during the drag.
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_TRUE(GetItemViewAt(i)->layer());
  apps_grid_view_->EndDrag(false);

  // The layer should be destroyed after the dragging.
  test_api_->WaitForItemMoveAnimationDone();
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    EXPECT_FALSE(GetItemViewAt(i)->layer());
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
  test_api_->LayoutToIdealBounds();

  // Dragging item_2 to the folder adds item_2 to the folder.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);

  EXPECT_EQ(kTotalItems - 2, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->GetModelContent());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  item_0 = model_->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  item_1 = model_->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  AppListItem* item_2 = model_->FindItem("Item 2");
  EXPECT_TRUE(item_2->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_2->folder_id());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, MouseDragMaxItemsInFolder) {
  // Create and add a folder with |kMaxFolderItemsFullscreen - 1| items.
  const size_t kMaxItems =
      AppListConfig::instance().max_folder_items_per_page() *
      AppListConfig::instance().max_folder_pages();
  const size_t kTotalItems = kMaxItems - 1;
  AppListFolderItem* folder_item =
      model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());

  // Create and add another 2 items.
  model_->PopulateAppWithId(kTotalItems);
  model_->PopulateAppWithId(kTotalItems + 1);
  EXPECT_EQ(3u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(model_->GetItemName(kMaxItems - 1),
            model_->top_level_item_list()->item_at(1)->id());
  EXPECT_EQ(model_->GetItemName(kMaxItems),
            model_->top_level_item_list()->item_at(2)->id());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();

  // Dragging one item into the folder, the folder should accept the item.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(kMaxItems, folder_item->ChildItemCount());
  EXPECT_EQ(model_->GetItemName(kMaxItems),
            model_->top_level_item_list()->item_at(1)->id());
  test_api_->LayoutToIdealBounds();

  // Dragging the last item over the folder, the folder won't accept the new
  // item.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxItems, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

// Check that moving items around doesn't allow a drop to happen into a full
// folder.
TEST_P(AppsGridViewTest, MouseDragMaxItemsInFolderWithMovement) {
  // Create and add a folder with |kMaxFolderItemsFullscreen| in it.
  const size_t kMaxItems =
      AppListConfig::instance().max_folder_items_per_page() *
      AppListConfig::instance().max_folder_pages();
  size_t kTotalItems = kMaxItems;
  model_->CreateAndPopulateFolderWithApps(kMaxItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());

  // Create and add another item.
  model_->PopulateAppWithId(kTotalItems);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(model_->GetItemName(kMaxItems),
            model_->top_level_item_list()->item_at(1)->id());

  AppListItemView* folder_view =
      GetItemViewForPoint(GetItemRectOnCurrentPageAt(0, 0).CenterPoint());

  // Drag the new item to the left so that the grid reorders.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).bottom_left();
  to.Offset(0, -1);  // Get a point inside the rect.
  AppListItemView* dragged_view = SimulateDrag(AppsGridView::MOUSE, from, to);
  test_api_->LayoutToIdealBounds();

  // The grid now looks like | blank | folder |.
  EXPECT_EQ(nullptr, GetItemViewForPoint(
                         GetItemRectOnCurrentPageAt(0, 0).CenterPoint()));
  EXPECT_EQ(folder_view, GetItemViewForPoint(
                             GetItemRectOnCurrentPageAt(0, 1).CenterPoint()));

  // Move onto the folder and end the drag.
  to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point translated_to =
      gfx::PointAtOffsetFromOrigin(to - dragged_view->origin());
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated_to, to,
                            ui::EventTimeForNow(), 0, 0);
  apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, drag_event);
  apps_grid_view_->EndDrag(false);

  // The item should not have moved into the folder.
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxItems, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

// Test reordering items via dragging.
TEST_P(AppsGridViewTest, MouseDragItemReorder) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  model_->PopulateApps(7);
  contents_view_->apps_container_view()->Layout();
  EXPECT_EQ(7u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3,Item 4,Item 5,Item 6"),
            model_->GetModelContent());

  // Dragging an item towards its neighbours should not reorder until the drag
  // is past the folder drop point.
  gfx::Point top_right = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Vector2d drag_vector;
  int half_tile_width = (GetItemRectOnCurrentPageAt(0, 1).x() -
                         GetItemRectOnCurrentPageAt(0, 0).x()) /
                        2;
  int tile_height = GetItemRectOnCurrentPageAt(1, 0).y() -
                    GetItemRectOnCurrentPageAt(0, 0).y();

  // Drag left but stop before the folder dropping circle.
  drag_vector.set_x(-half_tile_width - 4);
  SimulateDrag(AppsGridView::MOUSE, top_right, top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3,Item 4,Item 5,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();

  // Drag left, past the folder dropping circle.
  gfx::Vector2d last_drag_vector(drag_vector);
  drag_vector.set_x(-2 * half_tile_width -
                    AppListConfig::instance().folder_dropping_circle_radius() -
                    4);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 2,Item 3,Item 4,Item 5,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();

  // Drag down, between apps 5 and 6. The gap should open up, making space for
  // app 1 in the bottom left.
  last_drag_vector = drag_vector;
  drag_vector.set_x(-half_tile_width);
  drag_vector.set_y(tile_height);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 1,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();

  // Drag up, between apps 0 and 2. The gap should open up, making space for app
  // 1 in the top right.
  last_drag_vector = drag_vector;
  drag_vector.set_x(-half_tile_width);
  drag_vector.set_y(0);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3,Item 4,Item 5,Item 6"),
            model_->GetModelContent());
  TestAppListItemViewIndice();

  // Dragging down past the last app should reorder to the last position.
  last_drag_vector = drag_vector;
  drag_vector.set_x(half_tile_width);
  drag_vector.set_y(2 * tile_height);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 4,Item 5,Item 6,Item 1"),
            model_->GetModelContent());
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewTest, MouseDragFolderReorder) {
  size_t kTotalItems = 2;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  model_->PopulateAppWithId(kTotalItems);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ("Item 2", model_->top_level_item_list()->item_at(1)->id());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  // Dragging folder over item_1 should leads to re-ordering these two
  // items.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ("Item 2", model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(1)->id());
  test_api_->LayoutToIdealBounds();
  TestAppListItemViewIndice();
}

TEST_P(AppsGridViewTest, MouseDragWithCancelDeleteAddItem) {
  size_t kTotalItems = 4;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(model_->top_level_item_list()->item_count(), kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  // Canceling drag should keep existing order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(true);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Deleting an item keeps remaining intact.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  model_->DeleteItem(model_->GetItemName(2));
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 3"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Adding a launcher item cancels the drag and respects the order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  model_->CreateAndAddItem("Extra");
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 3,Extra"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// Test that control+arrow swaps app within the same page.
TEST_F(AppsGridViewTest, ControlArrowSwapsAppsWithinSamePage) {
  model_->PopulateApps(GetTilesPerPage(0));

  AppListItemView* moving_item = GetItemViewAt(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Test that moving left from 0,0 does not move the app.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, test_api_->GetViewAtVisualIndex(0, 0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving up from 0,0 does not move the app.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving right from 0,0 results in a swap with the item adjacent.
  AppListItemView* swapped_item = GetItemViewAt(1);
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(1));
  EXPECT_EQ(swapped_item, GetItemViewAt(0));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving down from 0,1 results in a swap with the item at 1,1.
  swapped_item = GetItemViewAt(apps_grid_view_->cols() + 1);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(apps_grid_view_->cols() + 1));
  EXPECT_EQ(swapped_item, GetItemViewAt(1));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving left from 1,1 results in a swap with the item at 1,0.
  swapped_item = GetItemViewAt(apps_grid_view_->cols());
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(apps_grid_view_->cols()));
  EXPECT_EQ(swapped_item, GetItemViewAt(apps_grid_view_->cols() + 1));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));

  // Test that moving up from 1,0 results in a swap with the item at 0,0.
  swapped_item = GetItemViewAt(0);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(0));
  EXPECT_EQ(swapped_item, GetItemViewAt(apps_grid_view_->cols()));
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(moving_item));
}

// Tests that histograms are recorded when apps are moved with control+arrow.
TEST_F(AppsGridViewTest, ControlArrowRecordsHistogramBasic) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(0));

  AppListItemView* moving_item = GetItemViewAt(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 2);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 3);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 4);
}

// Test that histograms do not record when the keyboard move is a no-op.
TEST_F(AppsGridViewTest, ControlArrowDoesNotRecordHistogramWithNoOpMove) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(0));

  AppListItemView* moving_item = GetItemViewAt(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Make 2 no-op moves and one successful move from 0,0 ane expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);
}

// Tests that histograms only record once for a long move sequence.
TEST_F(AppsGridViewTest, ControlArrowRecordsHistogramOnceWithOneMoveSequence) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(1) * 2);

  AppListItemView* moving_item = GetItemViewAt(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Make that a series of moves when the control key is left pressed and expect
  // one histogram is recorded.
  while (GetPaginationModel()->selected_page() != 1) {
    SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
    SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  }
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);
}

// Tests that moving an app down when it is directly below a gap results in a
// swap with the closest item.
TEST_F(AppsGridViewTest, ControlArrowDownToGapOnSamePage) {
  // Add two rows of apps, one full and one with just one app.
  model_->PopulateApps(apps_grid_view_->cols() + 1);

  // Select the far right item.
  AppListItemView* moving_item = GetItemViewAt(apps_grid_view_->cols() - 1);
  AppListItemView* swapped_item = GetItemViewAt(apps_grid_view_->cols());
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Press down to move the app to the next row. It should take the place of the
  // app on the next row that is closes to the column of |moving_item|.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(moving_item, GetItemViewAt(apps_grid_view_->cols()));
  EXPECT_EQ(swapped_item, GetItemViewAt(apps_grid_view_->cols() - 1));
}

// Tests that moving an app up/down/left/right to a full page results in the app
// at the destination slot moving to the source slot (ie. a swap).
TEST_F(AppsGridViewTest, ControlArrowSwapsBetweenFullPages) {
  const int kPages = 3;
  model_->PopulateApps(kPages * GetTilesPerPage(0));
  // For every item in the first row, ensure an upward move results in the item
  // swapping places with the item directly above it.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    GetPaginationModel()->SelectPage(1, false /*animate*/);
    const GridIndex moved_view_index(1, i);
    apps_grid_view_->GetFocusManager()->SetFocusedView(
        test_api_->GetViewAtIndex(moved_view_index));

    const GridIndex swapped_view_index(
        0,
        apps_grid_view_->cols() * (apps_grid_view_->rows_per_page() - 1) + i);
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
    GetPaginationModel()->SelectPage(1, false /*animate*/);
    const GridIndex moved_view_index(
        0,
        apps_grid_view_->cols() * (apps_grid_view_->rows_per_page() - 1) + i);
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
  GridIndex moved_view_index(
      0, apps_grid_view_->cols() * apps_grid_view_->rows_per_page() - 1);
  GridIndex swapped_view_index(1, 0);
  AppListItemView* moved_view = test_api_->GetViewAtIndex(moved_view_index);
  AppListItemView* swapped_view = test_api_->GetViewAtIndex(swapped_view_index);
  apps_grid_view_->GetFocusManager()->SetFocusedView(
      test_api_->GetViewAtIndex(moved_view_index));

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
  EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
  EXPECT_EQ(1, GetPaginationModel()->selected_page());

  // For the first item on the second page, moving left to a full page should
  // swap with the first item on the previous page.
  swapped_view_index = moved_view_index;
  moved_view_index = GridIndex(1, 0);
  moved_view = test_api_->GetViewAtIndex(moved_view_index);
  swapped_view = test_api_->GetViewAtIndex(swapped_view_index);

  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(swapped_view, test_api_->GetViewAtIndex(moved_view_index));
  EXPECT_EQ(moved_view, test_api_->GetViewAtIndex(swapped_view_index));
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
}

// Test that a page can be created while moving apps with the control+arrow.
TEST_F(AppsGridViewTest, ControlArrowDownAndRightCreatesNewPage) {
  base::HistogramTester histogram_tester;
  const int kTilesPerPageStart = GetTilesPerPage(0);
  model_->PopulateApps(kTilesPerPageStart);

  // Focus the last item on the page.
  AppListItemView* moving_item = GetItemViewAt(kTilesPerPageStart - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  // Test that pressing control-right creates a new page.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 1);
  EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(1, 0)));
  EXPECT_EQ(kTilesPerPageStart - 1, test_api_->AppsOnPage(0));
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Reset by moving the app back to the previous page.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 2);

  // Test that control-down creates a new page.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);

  // The slot where |moving_item| originated should be empty because items get
  // dumped on pages with room, and only swap if the destination page is full.
  EXPECT_EQ(
      nullptr,
      test_api_->GetViewAtIndex(GridIndex(
          0, apps_grid_view_->cols() * apps_grid_view_->rows_per_page() - 1)));
  EXPECT_EQ(moving_item, test_api_->GetViewAtIndex(GridIndex(1, 0)));
  EXPECT_EQ(kTilesPerPageStart - 1, test_api_->AppsOnPage(0));
  EXPECT_EQ(1, test_api_->AppsOnPage(1));
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 3);
}

// Tests that a page can be deleted if a lonely app is moved down or right to
// another page.
TEST_F(AppsGridViewTest, ControlArrowUpOrLeftRemovesPage) {
  base::HistogramTester histogram_tester;
  // Move an app so it is by itself on page 1.
  model_->PopulateApps(GetTilesPerPage(0));
  AppListItemView* moving_item = GetItemViewAt(GetTilesPerPage(0) - 1);
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
TEST_F(AppsGridViewTest, ControlArrowDownOnLastAppOnLastPage) {
  base::HistogramTester histogram_tester;
  // Move an app so it is by itself on page 1.
  model_->PopulateApps(GetTilesPerPage(0));
  AppListItemView* moving_item = GetItemViewAt(GetTilesPerPage(0) - 1);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 1);
  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Move the app right, test that nothing changes and no histograms are
  // recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 1);
  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());

  // Move the app down, test that nothing changes and no histograms are
  // recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListPageSwitcherSource", 7, 1);
  histogram_tester.ExpectBucketCount("Apps.AppListAppMovingType", 6, 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
}

// Test that moving an item down or right when it is by itself on a page with a
// page below results in the page deletion.
TEST_F(AppsGridViewTest, ControlArrowDownOrRightRemovesPage) {
  // Move an app so it is by itself on page 1, with another app on page 2.
  model_->PopulateApps(GetTilesPerPage(0));
  AppListItemView* moving_item = GetItemViewAt(GetTilesPerPage(0) - 1);
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
TEST_F(AppsGridViewTest, ControlShiftArrowFoldersItemBasic) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(0));
  // Select the first item in the grid, folder it with the item to the right.
  AppListItemView* first_item = GetItemViewAt(0);
  const std::string first_item_id = first_item->item()->id();
  const std::string second_item_id = GetItemViewAt(1)->item()->id();
  apps_grid_view_->GetFocusManager()->SetFocusedView(first_item);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items, and that the folder is the selected view.
  AppListItemView* new_folder = GetItemViewAt(0);
  ASSERT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
  histogram_tester.ExpectBucketCount(kAppListAppMovingType,
                                     kMoveByKeyboardIntoFolder, 1);

  // Test that, when a folder is selected, control+shift+arrow does nothing.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(kAppListAppMovingType,
                                     kMoveByKeyboardIntoFolder, 1);

  // Move selection to the item to the right of the folder and put it in the
  // folder.
  apps_grid_view_->GetFocusManager()->SetFocusedView(GetItemViewAt(1));

  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(kAppListAppMovingType,
                                     kMoveByKeyboardIntoFolder, 2);

  // Move selection to the item below the folder and put it in the folder.
  SimulateKeyPress(ui::VKEY_DOWN);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(kAppListAppMovingType,
                                     kMoveByKeyboardIntoFolder, 3);

  // Move the folder to the second row, then put the item above the folder in
  // the folder.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  EXPECT_EQ(5u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount(kAppListAppMovingType,
                                     kMoveByKeyboardIntoFolder, 4);
}

// Tests that foldering an item that is on a different page fails.
TEST_F(AppsGridViewTest, ControlShiftArrowFailsToFolderAcrossPages) {
  model_->PopulateApps(2 * GetTilesPerPage(0));

  // For every item on the last row of the first page, test that foldering to
  // the next page fails.
  for (int i = 0; i < apps_grid_view_->cols(); ++i) {
    const GridIndex moved_view_index(
        0,
        apps_grid_view_->cols() * (apps_grid_view_->rows_per_page() - 1) + i);
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
  GridIndex moved_view_index(
      0, apps_grid_view_->cols() * apps_grid_view_->rows_per_page() - 1);
  AppListItemView* attempted_folder_view =
      test_api_->GetViewAtIndex(moved_view_index);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_EQ(attempted_folder_view, test_api_->GetViewAtIndex(moved_view_index));

  // Move to the second page and test that foldering up to a new page fails.
  SimulateKeyPress(ui::VKEY_DOWN);

  // Select the first item on the second page.
  moved_view_index = GridIndex(1, 0);
  attempted_folder_view = test_api_->GetViewAtIndex(moved_view_index);

  // Try to folder left to the previous page, it  should fail.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

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

// Tests that foldering the item on the last slot of a page doesn't crash.
TEST_F(AppsGridViewTest, ControlShiftArrowFolderLastItemOnPage) {
  const int kNumberOfApps = 4;
  model_->PopulateApps(kNumberOfApps);
  // Select the second to last item in the grid, folder it with the item to the
  // right.
  AppListItemView* moving_item = GetItemViewAt(kNumberOfApps - 2);
  const std::string first_item_id = moving_item->item()->id();
  const std::string second_item_id =
      GetItemViewAt(kNumberOfApps - 1)->item()->id();
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items, and that the folder is the selected view.
  AppListItemView* new_folder = GetItemViewAt(kNumberOfApps - 2);
  ASSERT_TRUE(apps_grid_view_->IsSelectedView(new_folder));
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
}

// Flaky: crbug.com/1156634
TEST_P(AppsGridViewTest, DISABLED_MouseDragFlipPage) {
  apps_grid_view_->set_page_flip_delay_in_ms_for_testing(10);
  GetPaginationModel()->SetTransitionDurations(
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(10));

  PageFlipWaiter page_flip_waiter(GetPaginationModel());

  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to;
  const gfx::Rect apps_grid_bounds = apps_grid_view_->GetLocalBounds();
  to = gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() + 1);

  // For fullscreen/bubble launcher, drag to the bottom/right of bounds.
  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  EXPECT_EQ(to, GetDragViewCenter());

  // Page should be flipped after sometime to hit page 1 and 2 then stop.
  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }

  // When apps grid gap is enabled, the user can drag an item to an extra page
  // created at the end.
  EXPECT_EQ("1,2,3", page_flip_waiter.selected_pages());
  EXPECT_EQ(3, GetPaginationModel()->selected_page());
  EXPECT_EQ(to, GetDragViewCenter());

  // Cancel drag and put the dragged view back to its ideal position so that
  // the next drag would pick it up.
  apps_grid_view_->EndDrag(true);
  test_api_->LayoutToIdealBounds();

  // Now drag to the top edge, and test the other direction.
  to.set_y(apps_grid_bounds.y());

  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  EXPECT_EQ(to, GetDragViewCenter());

  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }

  EXPECT_EQ("1,0", page_flip_waiter.selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(to, GetDragViewCenter());

  apps_grid_view_->EndDrag(true);
}

TEST_F(AppsGridViewTest, UpdateFolderBackgroundOnCancelDrag) {
  const int kTotalItems = 4;
  TestAppsGridViewFolderDelegate folder_delegate;
  apps_grid_view_->set_folder_delegate(&folder_delegate);
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point mouse_from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point mouse_to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();

  // Starts a mouse drag and then cancels it.
  SimulateDrag(AppsGridView::MOUSE, mouse_from, mouse_to);
  apps_grid_view_->EndDrag(true);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());
}

// Test focus change before and after dragging an item. (See
// https://crbug.com/834682)
TEST_F(AppsGridViewTest, FocusOfDraggedView) {
  model_->PopulateApps(1);
  contents_view_->apps_container_view()->Layout();
  auto* search_box = contents_view_->GetSearchBoxView()->search_box();
  auto* item_view = apps_grid_view_->view_model()->view_at(0);
  EXPECT_TRUE(search_box->HasFocus());
  EXPECT_FALSE(item_view->HasFocus());

  // Dragging the item towards its right.
  const gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  const gfx::Point to = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  SimulateDrag(AppsGridView::MOUSE, from, to);
  EXPECT_FALSE(search_box->HasFocus());
  EXPECT_TRUE(item_view->HasFocus());

  apps_grid_view_->EndDrag(false);
  EXPECT_FALSE(search_box->HasFocus());
  EXPECT_TRUE(item_view->HasFocus());
}

class AppsGridViewTabletTest : public AppsGridViewTest {
 public:
  AppsGridViewTabletTest() : AppsGridViewTest(/*is_in_tablet=*/true) {}
  ~AppsGridViewTabletTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTabletTest);
};

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

// Make sure that a folder icon resets background blur after scrolling the apps
// grid without completing any transition (See https://crbug.com/1049275). The
// background blur is masked by the apps grid's layer mask.
TEST_F(AppsGridViewTabletTest, EnsureBlurAfterScrollingWithoutTransition) {
  // Create a folder with 2 apps. Then add apps until a second page is created.
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

  AppListItemView* folder_view = GetItemViewAt(0);
  ASSERT_TRUE(folder_view->is_folder());
  ASSERT_FALSE(apps_grid_view_->layer()->layer_mask_layer());

  // On the first page drag upwards, there should not be a page switch and the
  // layer mask should make the folder lose blur.
  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  apps_grid_view_->OnGestureEvent(&scroll_update_upwards);
  EXPECT_TRUE(scroll_update_upwards.handled());

  ASSERT_EQ(0, GetPaginationModel()->selected_page());
  ASSERT_TRUE(apps_grid_view_->layer()->layer_mask_layer());

  // Continue drag, now switching directions and release. There shouldn't be any
  // transition and the mask layer should've been reset.
  apps_grid_view_->OnGestureEvent(&scroll_update_downwards);
  EXPECT_TRUE(scroll_update_downwards.handled());
  apps_grid_view_->OnGestureEvent(&scroll_end);
  EXPECT_TRUE(scroll_end.handled());

  EXPECT_FALSE(GetPaginationModel()->has_transition());
  EXPECT_FALSE(apps_grid_view_->layer()->layer_mask_layer());
}

INSTANTIATE_TEST_SUITE_P(All, AppsGridViewTabletTest, testing::Bool());

// Test various dragging behaviors only allowed when apps grid gap (part of
// home launcher feature) is enabled.
class AppsGridGapTest : public AppsGridViewTest {
 public:
  AppsGridGapTest() = default;
  ~AppsGridGapTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    AppsGridViewTest::SetUp();
    apps_grid_view_->set_page_flip_delay_in_ms_for_testing(10);
    GetPaginationModel()->SetTransitionDurations(
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromMilliseconds(10));
    page_flip_waiter_ = std::make_unique<PageFlipWaiter>(GetPaginationModel());
  }

  void TearDown() override {
    page_flip_waiter_.reset();
    AppsGridViewTest::TearDown();
  }

 protected:
  // Simulate drag from the |from| point to either next or previous page's |to|
  // point.
  void SimulateDragToNeighborPage(bool next_page,
                                  const gfx::Point& from,
                                  const gfx::Point& to) {
    const int selected_page = GetPaginationModel()->selected_page();
    DCHECK(selected_page >= 0 &&
           selected_page <= GetPaginationModel()->total_pages());

    // Calculate the point required to flip the page if an item is dragged to
    // it.
    const gfx::Rect apps_grid_bounds = apps_grid_view_->GetLocalBounds();
    gfx::Point point_in_page_flip_buffer =
        gfx::Point(apps_grid_bounds.width() / 2,
                   next_page ? apps_grid_bounds.bottom() + 1 : 0);

    // Build the drag event which will be triggered after page flip.
    gfx::Point root_to(to);
    views::View::ConvertPointToWidget(apps_grid_view_, &root_to);
    gfx::NativeWindow window = app_list_view_->GetWidget()->GetNativeWindow();
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &root_to);
    root_to.set_x(apps_grid_view_->GetMirroredXInView(root_to.x()));
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, to, root_to,
                              ui::EventTimeForNow(), 0, 0);

    // Start dragging and relayout apps grid view after drag ends.
    DragAfterPageFlipTask task(GetPaginationModel(), apps_grid_view_,
                               drag_event);
    page_flip_waiter_->Reset();
    SimulateDrag(AppsGridView::MOUSE, from, point_in_page_flip_buffer);
    while (test_api_->HasPendingPageFlip()) {
      page_flip_waiter_->Wait();
    }
    apps_grid_view_->EndDrag(false);
    test_api_->LayoutToIdealBounds();
  }

  std::unique_ptr<PageFlipWaiter> page_flip_waiter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridGapTest);
};

INSTANTIATE_TEST_SUITE_P(All, AppsGridGapTest, testing::Bool());

TEST_P(AppsGridGapTest, MoveAnItemToNewEmptyPage) {
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

  // Drag the first item to the page bottom.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);

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
}

TEST_P(AppsGridGapTest, MoveLastItemToCreateFolderInNextPage) {
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

  // Drag the first item to next page and drag the second item to overlap with
  // the first item.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);

  // A new folder is created on second page, but since the first page is empty,
  // the page is removed and the new folder ends up on first page.
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
}

TEST_P(AppsGridGapTest, MoveLastItemForReorderInNextPage) {
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

  // Drag the first item to next page and drag the second item to the left of
  // the first item.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(1, 0);
  gfx::Point to_in_next_page = tile_rect.CenterPoint();
  to_in_next_page.set_x(tile_rect.x());
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);

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
}

TEST_P(AppsGridGapTest, MoveLastItemToNewEmptyPage) {
  const int kApps = 1;
  model_->PopulateApps(kApps);

  // There's only one page and only one item in that page.
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(1, view_model->view_size());
  EXPECT_EQ(view_model->view_at(0),
            test_api_->GetViewAtVisualIndex(0 /* page */, 0 /* slot */));
  EXPECT_EQ("Item 0", view_model->view_at(0)->item()->id());
  EXPECT_EQ(std::string("Item 0"), model_->GetModelContent());

  // Drag the item to next page.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to_in_next_page =
      test_api_->GetItemTileRectAtVisualIndex(1, 0).CenterPoint();
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);
  GetPaginationModel()->SelectPage(0, false);
  SimulateDragToNeighborPage(true /* next_page */, from, to_in_next_page);

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
}

TEST_P(AppsGridGapTest, MoveItemToPreviousFullPage) {
  const int kApps = 2 + GetTilesPerPage(0);
  model_->PopulateApps(kApps);

  // There are two pages and last item is on second page.
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  TestAppListItemViewIndice();
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view_->view_model();
  EXPECT_EQ(kApps, view_model->view_size());
  for (int i = 0; i < kApps; ++i) {
    EXPECT_EQ(view_model->view_at(i), test_api_->GetViewAtVisualIndex(
                                          i / GetTilesPerPage(0) /* page */,
                                          i % GetTilesPerPage(0) /* slot */));
    EXPECT_EQ("Item " + base::NumberToString(i),
              view_model->view_at(i)->item()->id());
  }

  // There's no "page break" item at the end of first page, although there are
  // two pages. It will only be added after user operations.
  std::string model_content = "Item 0";
  for (int i = 1; i < kApps; ++i)
    model_content.append(",Item " + base::NumberToString(i));
  EXPECT_EQ(model_content, model_->GetModelContent());

  // Drag the last item to the first item's left position in previous page.
  gfx::Point from = test_api_->GetItemTileRectAtVisualIndex(1, 1).CenterPoint();
  gfx::Rect tile_rect = test_api_->GetItemTileRectAtVisualIndex(0, 0);
  gfx::Point to_in_previous_page = tile_rect.CenterPoint();
  to_in_previous_page.set_x(tile_rect.x());
  GetPaginationModel()->SelectPage(1, false);
  SimulateDragToNeighborPage(false /* next_page */, from, to_in_previous_page);

  // The dragging is successful, the last item becomes the first item.
  EXPECT_EQ("0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(kApps, view_model->view_size());
  for (int i = 0; i < kApps; ++i) {
    EXPECT_EQ(view_model->view_at(i), test_api_->GetViewAtVisualIndex(
                                          i / GetTilesPerPage(0) /* page */,
                                          i % GetTilesPerPage(0) /* slot */));
    EXPECT_EQ("Item " + base::NumberToString((i + kApps - 1) % kApps),
              view_model->view_at(i)->item()->id());
  }

  // A "page break" item is added to split the pages.
  model_content = "Item " + base::NumberToString(kApps - 1);
  for (int i = 1; i < kApps; ++i) {
    model_content.append(",Item " + base::NumberToString(i - 1));
    if (i == GetTilesPerPage(0) - 1)
      model_content.append(",PageBreakItem");
  }
  EXPECT_EQ(model_content, model_->GetModelContent());

  // Again drag the last item to the first item's left position in previous
  // page.
  GetPaginationModel()->SelectPage(1, false);
  SimulateDragToNeighborPage(false /* next_page */, from, to_in_previous_page);

  // The dragging is successful, the last item becomes the first item again.
  EXPECT_EQ("0", page_flip_waiter_->selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  TestAppListItemViewIndice();
  EXPECT_EQ(kApps, view_model->view_size());
  for (int i = 0; i < kApps; ++i) {
    EXPECT_EQ(view_model->view_at(i), test_api_->GetViewAtVisualIndex(
                                          i / GetTilesPerPage(0) /* page */,
                                          i % GetTilesPerPage(0) /* slot */));
    EXPECT_EQ("Item " + base::NumberToString((i + kApps - 2) % kApps),
              view_model->view_at(i)->item()->id());
  }

  // A "page break" item still exists.
  model_content = "Item " + base::NumberToString(kApps - 2) + ",Item " +
                  base::NumberToString(kApps - 1);
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
  AppListItemView* moving_item = GetItemViewAt(0);
  apps_grid_view_->GetFocusManager()->SetFocusedView(moving_item);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  ASSERT_EQ(apps_grid_view_->pagination_model()->total_pages(), 2);
  histogram_tester.ExpectBucketCount(
      "Apps.AppList.AppsGridAddPage",
      AppListPageCreationType::kMovingAppWithKeyboard, 1);
}

TEST_F(AppsGridViewTest, CreateANewPageByDraggingLogsMetrics) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(2);

  PageFlipWaiter page_flip_waiter(GetPaginationModel());
  // Drag down the first item until a new page is created.
  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  const gfx::Rect apps_grid_bounds = apps_grid_view_->GetLocalBounds();
  gfx::Point to =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() + 1);

  // For fullscreen, drag to the bottom/right of bounds.
  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  EXPECT_EQ(to, GetDragViewCenter());

  while (test_api_->HasPendingPageFlip())
    page_flip_waiter.Wait();

  apps_grid_view_->EndDrag(false /*cancel*/);

  ASSERT_EQ(apps_grid_view_->pagination_model()->total_pages(), 2);
  histogram_tester.ExpectBucketCount("Apps.AppList.AppsGridAddPage",
                                     AppListPageCreationType::kDraggingApp, 1);
}

TEST_F(AppsGridViewTest, CreateANewPageByAddingAppLogsMetrics) {
  base::HistogramTester histogram_tester;
  model_->PopulateApps(GetTilesPerPage(0));

  // Add an item to simulate installing or syncing, the metric should be
  // recorded.
  model_->CreateAndAddItem("Extra App");

  ASSERT_EQ(apps_grid_view_->pagination_model()->total_pages(), 2);
  histogram_tester.ExpectBucketCount("Apps.AppList.AppsGridAddPage",
                                     AppListPageCreationType::kSyncOrInstall,
                                     1);
}

}  // namespace test
}  // namespace ash
