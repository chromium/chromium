// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/pagination_model.h"
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
#include "ash/app_list/views/suggestions_container_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {

constexpr int kNumOfSuggestedApps = 3;

class PageFlipWaiter : public PaginationModelObserver {
 public:
  explicit PageFlipWaiter(PaginationModel* model) : model_(model) {
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
  void TotalPagesChanged() override {}
  void SelectedPageChanged(int old_selected, int new_selected) override {
    if (!selected_pages_.empty())
      selected_pages_ += ',';
    selected_pages_ += base::IntToString(new_selected);

    if (wait_)
      ui_run_loop_->QuitWhenIdle();
  }
  void TransitionStarted() override {}
  void TransitionChanged() override {}
  void TransitionEnded() override {}

  std::unique_ptr<base::RunLoop> ui_run_loop_;
  PaginationModel* model_ = nullptr;
  bool wait_ = false;
  std::string selected_pages_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipWaiter);
};

// Dragging task to be run after page flip is observed.
class DragAfterPageFlipTask : public PaginationModelObserver {
 public:
  DragAfterPageFlipTask(PaginationModel* model,
                        AppsGridView* view,
                        const ui::MouseEvent& drag_event)
      : model_(model), view_(view), drag_event_(drag_event) {
    model_->AddObserver(this);
  }

  ~DragAfterPageFlipTask() override { model_->RemoveObserver(this); }

 private:
  // PaginationModelObserver overrides:
  void TotalPagesChanged() override {}
  void SelectedPageChanged(int old_selected, int new_selected) override {
    view_->UpdateDragFromItem(AppsGridView::MOUSE, drag_event_);
  }
  void TransitionStarted() override {}
  void TransitionChanged() override {}
  void TransitionEnded() override {}

  PaginationModel* model_;
  AppsGridView* view_;
  ui::MouseEvent drag_event_;

  DISALLOW_COPY_AND_ASSIGN(DragAfterPageFlipTask);
};

class TestSuggestedSearchResult : public TestSearchResult {
 public:
  TestSuggestedSearchResult() {
    set_display_type(ash::SearchResultDisplayType::kRecommendation);
  }
  ~TestSuggestedSearchResult() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSuggestedSearchResult);
};

struct TestParams {
  bool is_rtl_enabled;
  bool is_apps_grid_gap_enabled;
  bool is_new_style_launcher_enabled;
};

const TestParams kAppsGridViewTestParams[] = {
    {false /* is_rtl_enabled */, false /* is_apps_grid_gap_enabled */,
     false /* is_new_style_launcher_enabled */},
    {false, false, true},
    {true, false, false},
    {true, false, true},
};

const TestParams kAppsGridViewDragTestParams[] = {
    {false /* is_rtl_enabled */, false /* is_apps_grid_gap_enabled */,
     false /* is_new_style_launcher_enabled */},
    {false, false, true},
    {true, false, false},
    {true, false, true},
    {false, true, false},
    {false, true, true},
    {true, true, false},
    {true, true, true},
};

const TestParams kAppsGridGapTestParams[] = {
    {false /* is_rtl_enabled */, true /* is_apps_grid_gap_enabled */,
     false /* is_new_style_launcher_enabled */},
    {false, true, true},
    {true, true, false},
    {true, true, true},
};

}  // namespace

class AppsGridViewTest : public views::ViewsTestBase,
                         public testing::WithParamInterface<TestParams> {
 public:
  AppsGridViewTest() = default;
  ~AppsGridViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      is_rtl_ = GetParam().is_rtl_enabled;
      if (is_rtl_)
        base::i18n::SetICUDefaultLocale("he");

      is_apps_grid_gap_enabled_ = GetParam().is_apps_grid_gap_enabled;
      is_new_style_launcher_enabled_ = GetParam().is_new_style_launcher_enabled;
    }
    if (is_apps_grid_gap_enabled_) {
      enabled_features.emplace_back(
          app_list_features::kEnableAppsGridGapFeature);
    } else {
      disabled_features.emplace_back(
          app_list_features::kEnableAppsGridGapFeature);
    }
    if (is_new_style_launcher_enabled_) {
      enabled_features.emplace_back(app_list_features::kEnableNewStyleLauncher);
    } else {
      disabled_features.emplace_back(
          app_list_features::kEnableNewStyleLauncher);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    views::ViewsTestBase::SetUp();
    gfx::NativeView parent = GetContext();
    // Ensure that parent is big enough to show the full AppListView.
    parent->SetBounds(gfx::Rect(gfx::Point(0, 0), gfx::Size(1024, 768)));
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    app_list_view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = parent;
    app_list_view_->Initialize(params);
    contents_view_ = app_list_view_->app_list_main_view()->contents_view();
    apps_grid_view_ = contents_view_->GetAppsContainerView()->apps_grid_view();
    app_list_view_->GetWidget()->Show();

    model_ = delegate_->GetTestModel();
    search_model_ = delegate_->GetSearchModel();
    if (is_new_style_launcher_enabled_) {
      suggestions_container_ = contents_view_->GetAppsContainerView()
                                   ->suggestion_chip_container_view_for_test();
    } else {
      suggestions_container_ =
          apps_grid_view_->suggestions_container_for_test();
    }

    expand_arrow_view_ = apps_grid_view_->expand_arrow_view_for_test();
    for (size_t i = 0; i < kNumOfSuggestedApps; ++i) {
      search_model_->results()->Add(
          std::make_unique<TestSuggestedSearchResult>());
    }
    // Needed to update suggestions from |model_|.
    suggestions_container_->Update();
    app_list_view_->SetState(AppListViewState::FULLSCREEN_ALL_APPS);
    app_list_view_->Layout();

    test_api_ = std::make_unique<AppsGridViewTestApi>(apps_grid_view_);
  }
  void TearDown() override {
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
      if (view->bounds().Contains(point))
        return view;
    }
    return nullptr;
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(model_->top_level_item_list()->item_count(), 0u);
    return test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  int GetTilesPerPage(int page) const { return test_api_->TilesPerPage(page); }

  PaginationModel* GetPaginationModel() const {
    return apps_grid_view_->pagination_model();
  }

  AppListFolderView* app_list_folder_view() const {
    return contents_view_->GetAppsContainerView()->app_list_folder_view();
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

  // Tests that the order of item views in the AppsGridView is in accordance
  // with the order in the view model.
  void TestAppListItemViewIndice() {
    const views::ViewModelT<AppListItemView>* view_model =
        apps_grid_view_->view_model();
    DCHECK_GT(view_model->view_size(), 0);
    const int initial_index =
        apps_grid_view_->GetIndexOf(view_model->view_at(0));
    DCHECK_NE(-1, initial_index);
    for (int i = 0; i < view_model->view_size(); ++i) {
      EXPECT_EQ(view_model->view_at(i),
                apps_grid_view_->child_at(i + initial_index));
    }
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
  bool is_apps_grid_gap_enabled_ = false;
  bool is_new_style_launcher_enabled_ = false;

 private:
  // Restores the locale to default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  // Used by AppListFolderView::UpdatePreferredBounds.
  keyboard::KeyboardController keyboard_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTest);
};

// Instantiate the Boolean which is used to toggle RTL in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(,
                        AppsGridViewTest,
                        testing::ValuesIn(kAppsGridViewTestParams));

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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppsGridViewFolderDelegate);
};

TEST_P(AppsGridViewTest, CreatePage) {
  // Fully populates a page.
  const int kPages = 1;

  EXPECT_EQ(kNumOfSuggestedApps, suggestions_container_->num_results());
  // For new style launcher, each page has the same number of rows.
  const int kExpectedTilesOnFirstPage =
      apps_grid_view_->cols() * (apps_grid_view_->rows_per_page() -
                                 (is_new_style_launcher_enabled_ ? 0 : 1));
  EXPECT_EQ(kExpectedTilesOnFirstPage, GetTilesPerPage(kPages - 1));

  model_->PopulateApps(kPages * GetTilesPerPage(kPages - 1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());

  // Adds one more and gets a new page created.
  model_->CreateAndAddItem("Extra");
  EXPECT_EQ(kPages + 1, GetPaginationModel()->total_pages());
}

TEST_F(AppsGridViewTest, EnsureHighlightedVisible) {
  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Highlight first one and last one one first page and first page should be
  // selected.
  model_->HighlightItemAt(0);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  model_->HighlightItemAt(GetTilesPerPage(0) - 1);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Highlight first one on 2nd page and 2nd page should be selected.
  model_->HighlightItemAt(GetTilesPerPage(1) + 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());

  // Highlight last one in the model and last page should be selected.
  model_->HighlightItemAt(model_->top_level_item_list()->item_count() - 1);
  EXPECT_EQ(kPages - 1, GetPaginationModel()->selected_page());
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

  // Select the first suggested app and launch it.
  contents_view_->GetAppListMainView()->ActivateApp(GetItemViewAt(0)->item(),
                                                    0);

  // Test that histograms recorded that a regular app launched.
  histogram_tester.ExpectBucketCount("Apps.AppListAppLaunchedFullscreen", 0, 1);
  // Test that histograms did not record that a suggested launched.
  histogram_tester.ExpectBucketCount("Apps.AppListAppLaunchedFullscreen", 1, 0);

  // Launch a suggested app.
  suggestions_container_->child_at(0)->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));

  // Test that histograms recorded that a suggested app launched, and that the
  // count for regular apps launched is unchanged.
  histogram_tester.ExpectBucketCount("Apps.AppListAppLaunchedFullscreen", 0, 1);
  histogram_tester.ExpectBucketCount("Apps.AppListAppLaunchedFullscreen", 1, 1);
}

TEST_F(AppsGridViewTest, ItemLabelShortNameOverride) {
  // If the app's full name and short name differ, the title label's tooltip
  // should always be the full name of the app.
  std::string expected_text("xyz");
  std::string expected_tooltip("tooltip");
  AppListItem* item = model_->CreateAndAddItem("Item with short name");
  model_->SetItemNameAndShortName(item, expected_tooltip, expected_text);

  base::string16 actual_tooltip;
  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_TRUE(item_view->GetTooltipText(title_label->bounds().CenterPoint(),
                                        &actual_tooltip));
  EXPECT_EQ(expected_tooltip, base::UTF16ToUTF8(actual_tooltip));
  EXPECT_EQ(expected_text, base::UTF16ToUTF8(title_label->text()));
}

TEST_F(AppsGridViewTest, ItemLabelNoShortName) {
  // If the app's full name and short name are the same, use the default tooltip
  // behavior of the label (only show a tooltip if the title is truncated).
  std::string title("a");
  AppListItem* item = model_->CreateAndAddItem(title);
  model_->SetItemNameAndShortName(item, title, "");

  base::string16 actual_tooltip;
  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_FALSE(title_label->GetTooltipText(title_label->bounds().CenterPoint(),
                                           &actual_tooltip));
  EXPECT_EQ(title, base::UTF16ToUTF8(title_label->text()));
}

TEST_P(AppsGridViewTest, ScrollSequenceHandledByAppListView) {
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

  // Drag down on the app grid when on page 1, this should move the AppListView
  // and not move the AppsGridView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_FALSE(scroll_begin.handled());

  // Simulate redirecting the event to app list view through views hierarchy.
  app_list_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());

  // The following scroll update events will be sent to the view that handled
  // the scroll begin event.
  app_list_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_TRUE(app_list_view_->is_in_drag());
  ASSERT_EQ(0, GetPaginationModel()->transition().progress);
}

TEST_F(AppsGridViewTest,
       OnGestureEventScrollSequenceHandleByPaginationController) {
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

  // Drag up on the app grid when on page 1, this should move the AppsGridView
  // but not the AppListView.
  apps_grid_view_->OnGestureEvent(&scroll_begin);
  EXPECT_TRUE(scroll_begin.handled());
  apps_grid_view_->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_update.handled());
  ASSERT_FALSE(app_list_view_->is_in_drag());
  ASSERT_NE(0, GetPaginationModel()->transition().progress);
}

TEST_F(AppsGridViewTest, CloseFolderByClickingBackground) {
  AppsContainerView* apps_container_view =
      contents_view_->GetAppsContainerView();

  const size_t kTotalItems = kMaxFolderItemsPerPage;
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

TEST_F(AppsGridViewTest, PageResetAfterOpenFolder) {
  const size_t kTotalItems = kMaxFolderPages * kMaxFolderItemsPerPage;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder. It should be at page 0.
  test_api_->PressItemAt(0);
  PaginationModel* pagination_model =
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
  const size_t kTotalItems = kMaxFolderItemsPerPage;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  test_api_->PressItemAt(0);
  EXPECT_TRUE(contents_view_->GetAppsContainerView()->IsInFolderView());

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
  EXPECT_TRUE(contents_view_->GetAppsContainerView()->IsInFolderView());
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

// Tests various dragging behaviors.
class AppsGridViewDragTest : public AppsGridViewTest {
 public:
  AppsGridViewDragTest() = default;
  ~AppsGridViewDragTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridViewDragTest);
};

INSTANTIATE_TEST_CASE_P(,
                        AppsGridViewDragTest,
                        testing::ValuesIn(kAppsGridViewDragTestParams));

TEST_P(AppsGridViewDragTest, MouseDragItemIntoFolder) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(model_->top_level_item_list()->item_count(), kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2"), model_->GetModelContent());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 1).CenterPoint();
  gfx::Point to = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();

  // Dragging item_1 over item_0 creates a folder.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
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

TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolder) {
  // Create and add a folder with |kMaxFolderItemsFullscreen - 1| items.
  const size_t kMaxItems = kMaxFolderItemsPerPage * kMaxFolderPages;
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
TEST_P(AppsGridViewDragTest, MouseDragMaxItemsInFolderWithMovement) {
  // Create and add a folder with |kMaxFolderItemsFullscreen| in it.
  const size_t kMaxItems = kMaxFolderItemsPerPage * kMaxFolderPages;
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
TEST_P(AppsGridViewDragTest, MouseDragItemReorder) {
  // The default layout is 5x4, populate 7 apps so that we have second row to
  // test dragging item to second row.
  model_->PopulateApps(7);
  contents_view_->GetAppsContainerView()->Layout();
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

TEST_P(AppsGridViewDragTest, MouseDragFolderReorder) {
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

TEST_P(AppsGridViewDragTest, MouseDragWithCancelDeleteAddItem) {
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

TEST_P(AppsGridViewDragTest, MouseDragFlipPage) {
  apps_grid_view_->set_page_flip_delay_in_ms_for_testing(10);
  GetPaginationModel()->SetTransitionDurations(10, 10);

  PageFlipWaiter page_flip_waiter(GetPaginationModel());

  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  gfx::Point from = GetItemRectOnCurrentPageAt(0, 0).CenterPoint();
  gfx::Point to;
  const gfx::Rect apps_grid_bounds = apps_grid_view_->GetLocalBounds();
  to = gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom());

  // For fullscreen/bubble launcher, drag to the bottom/right of bounds.
  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  // Page should be flipped after sometime to hit page 1 and 2 then stop.
  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }

  if (is_apps_grid_gap_enabled_) {
    // When apps grid gap is enabled, the user can drag an item to an extra page
    // created at the end.
    EXPECT_EQ("1,2,3", page_flip_waiter.selected_pages());
    EXPECT_EQ(3, GetPaginationModel()->selected_page());
  } else {
    EXPECT_EQ("1,2", page_flip_waiter.selected_pages());
    EXPECT_EQ(2, GetPaginationModel()->selected_page());
  }

  // Cancel drag and put the dragged view back to its ideal position so that
  // the next drag would pick it up.
  apps_grid_view_->EndDrag(true);
  test_api_->LayoutToIdealBounds();

  // Now drag to the top edge, and test the other direction.
  to.set_y(apps_grid_bounds.y());

  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }

  EXPECT_EQ("1,0", page_flip_waiter.selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  apps_grid_view_->EndDrag(true);
}

TEST_F(AppsGridViewDragTest, UpdateFolderBackgroundOnCancelDrag) {
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

// Test various dragging behaviors only allowed when apps grid gap (part of
// home launcher feature) is enabled.
class AppsGridGapTest : public AppsGridViewTest {
 public:
  AppsGridGapTest() = default;
  ~AppsGridGapTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {app_list_features::kEnableAppsGridGapFeature}, {});
    AppsGridViewTest::SetUp();
    apps_grid_view_->set_page_flip_delay_in_ms_for_testing(10);
    GetPaginationModel()->SetTransitionDurations(10, 10);
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
                   next_page ? apps_grid_bounds.bottom() : 0);

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
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridGapTest);
};

INSTANTIATE_TEST_CASE_P(,
                        AppsGridGapTest,
                        testing::ValuesIn(kAppsGridGapTestParams));

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
    EXPECT_EQ("Item " + base::IntToString(i),
              view_model->view_at(i)->item()->id());
  }

  // There's no "page break" item at the end of first page, although there are
  // two pages. It will only be added after user operations.
  std::string model_content = "Item 0";
  for (int i = 1; i < kApps; ++i)
    model_content.append(",Item " + base::IntToString(i));
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
    EXPECT_EQ("Item " + base::IntToString((i + kApps - 1) % kApps),
              view_model->view_at(i)->item()->id());
  }

  // A "page break" item is added to split the pages.
  model_content = "Item " + base::IntToString(kApps - 1);
  for (int i = 1; i < kApps; ++i) {
    model_content.append(",Item " + base::IntToString(i - 1));
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
    EXPECT_EQ("Item " + base::IntToString((i + kApps - 2) % kApps),
              view_model->view_at(i)->item()->id());
  }

  // A "page break" item still exists.
  model_content = "Item " + base::IntToString(kApps - 2) + ",Item " +
                  base::IntToString(kApps - 1);
  for (int i = 2; i < kApps; ++i) {
    model_content.append(",Item " + base::IntToString(i - 2));
    if (i == GetTilesPerPage(0) - 1)
      model_content.append(",PageBreakItem");
  }
  EXPECT_EQ(model_content, model_->GetModelContent());
}

}  // namespace test
}  // namespace app_list
