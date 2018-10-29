// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/pagination_model.h"
#include "ash/app_list/pagination_model_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace gfx {
class Rect;
}

namespace app_list {

class AppListPage;
class AppListView;
class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListMainView;
class AppsContainerView;
class AppsGridView;
class ExpandArrowView;
class HorizontalPageContainer;
class PaginationModel;
class SearchBoxView;
class SearchResultAnswerCardView;
class SearchResultListView;
class SearchResultPageView;
class SearchResultTileItemListView;

// A view to manage launcher pages within the Launcher (eg. start page, apps
// grid view, search results). There can be any number of launcher pages, only
// one of which can be active at a given time. ContentsView provides the user
// interface for switching between launcher pages, and animates the transition
// between them.
class APP_LIST_EXPORT ContentsView : public views::View,
                                     public PaginationModelObserver {
 public:
  explicit ContentsView(AppListView* app_list_view);
  ~ContentsView() override;

  // Initialize the pages of the launcher. Should be called after
  // set_contents_switcher_view().
  void Init(AppListModel* model);

  // The app list gets closed and drag and drop operations need to be cancelled.
  void CancelDrag();

  // If |drag_and_drop| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Shows/hides the search results. Hiding the search results will cause the
  // app list to return to the page that was displayed before
  // ShowSearchResults(true) was invoked.
  void ShowSearchResults(bool show);
  bool IsShowingSearchResults() const;

  void ShowFolderContent(AppListFolderItem* folder);

  // Sets the active launcher page and animates the pages into place.
  void SetActiveState(ash::AppListState state);
  void SetActiveState(ash::AppListState state, bool animate);

  // The index of the currently active launcher page.
  int GetActivePageIndex() const;

  // The currently active state.
  ash::AppListState GetActiveState() const;

  // True if |state| is the current active laucher page.
  bool IsStateActive(ash::AppListState state) const;

  // Gets the index of a launcher page in |view_model_|, by State. Returns
  // -1 if there is no view for |state|.
  int GetPageIndexForState(ash::AppListState state) const;

  // Gets the state of a launcher page in |view_model_|, by index. Returns
  // INVALID_STATE if there is no state for |index|.
  ash::AppListState GetStateForPageIndex(int index) const;

  int NumLauncherPages() const;

  AppsContainerView* GetAppsContainerView();

  SearchResultPageView* search_results_page_view() const {
    return search_results_page_view_;
  }
  SearchResultAnswerCardView* search_result_answer_card_view_for_test() const {
    return search_result_answer_card_view_;
  }
  SearchResultTileItemListView* search_result_tile_item_list_view_for_test()
      const {
    return search_result_tile_item_list_view_;
  }
  SearchResultListView* search_result_list_view_for_test() const {
    return search_result_list_view_;
  }
  HorizontalPageContainer* horizontal_page_container() const {
    return horizontal_page_container_;
  }
  AppListPage* GetPageView(int index) const;

  SearchBoxView* GetSearchBoxView() const;

  AppListMainView* GetAppListMainView() const;

  AppListView* app_list_view() const { return app_list_view_; }

  ExpandArrowView* expand_arrow_view() const { return expand_arrow_view_; }

  // Returns the pagination model for the ContentsView.
  const PaginationModel& pagination_model() { return pagination_model_; }

  // Returns search box bounds to use for content views that do not specify
  // their own custom layout.
  gfx::Rect GetDefaultSearchBoxBounds() const;

  // Returns search box bounds to use for a given state.
  gfx::Rect GetSearchBoxBoundsForState(ash::AppListState state) const;

  // Returns the content area bounds to use for content views that do not
  // specify their own custom layout.
  gfx::Rect GetDefaultContentsBounds() const;

  // Returns the maximum preferred size of the all pages.
  gfx::Size GetMaximumContentsSize() const;

  // Performs the 'back' action for the active page. Returns whether the action
  // was handled.
  bool Back();

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;

  // Returns selected view in active page.
  views::View* GetSelectedView() const;

  // Updates y position and opacity of the items in this view during dragging.
  void UpdateYPositionAndOpacity();

 private:
  // Sets the active launcher page, accounting for whether the change is for
  // search results.
  void SetActiveStateInternal(int page_index,
                              bool show_search_results,
                              bool animate);

  // Invoked when active view is changed.
  void ActivePageChanged();

  // Returns the size of the default content area.
  gfx::Size GetDefaultContentsSize() const;

  // Calculates and sets the bounds for the subviews. If there is currently an
  // animation, this positions the views as appropriate for the current frame.
  void UpdatePageBounds();

  void UpdateSearchBox(double progress,
                       ash::AppListState current_state,
                       ash::AppListState target_state);

  // Updates the expand arrow's opacity based on the progress of transition from
  // current state to target state.
  void UpdateExpandArrowOpacity(double progress,
                                ash::AppListState current_state,
                                ash::AppListState target_state);

  // Updates the expand arrow's focus behavior based on the current state.
  void UpdateExpandArrowFocusBehavior(ash::AppListState current_state);

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView. There is no name
  // associated with the page. Returns the index of the new page.
  int AddLauncherPage(AppListPage* view);

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView. The page is associated
  // with the name |state|. Returns the index of the new page.
  int AddLauncherPage(AppListPage* view, ash::AppListState state);

  // Gets the PaginationModel owned by the AppsGridView.
  // Note: This is different to |pagination_model_|, which manages top-level
  // launcher-page pagination.
  PaginationModel* GetAppsPaginationModel();

  // Returns true if the |page| requires layout when transitioning from
  // |current_state| to |target_state|.
  bool ShouldLayoutPage(AppListPage* page,
                        ash::AppListState current_state,
                        ash::AppListState target_state) const;

  // Converts rect to widget without applying transform.
  gfx::Rect ConvertRectToWidgetWithoutTransform(const gfx::Rect& rect);

  // Unowned pointer to application list model.
  AppListModel* model_ = nullptr;

  // Sub-views of the ContentsView. All owned by the views hierarchy.
  HorizontalPageContainer* horizontal_page_container_ = nullptr;
  SearchResultPageView* search_results_page_view_ = nullptr;
  SearchResultAnswerCardView* search_result_answer_card_view_ = nullptr;
  SearchResultTileItemListView* search_result_tile_item_list_view_ = nullptr;
  SearchResultListView* search_result_list_view_ = nullptr;

  // The child page views. Owned by the views hierarchy.
  std::vector<AppListPage*> app_list_pages_;

  // Owned by the views hierarchy.
  AppListView* const app_list_view_;

  // Owned by the views hierarchy.
  ExpandArrowView* expand_arrow_view_ = nullptr;

  // Maps State onto |view_model_| indices.
  std::map<ash::AppListState, int> state_to_view_;

  // Maps |view_model_| indices onto State.
  std::map<int, ash::AppListState> view_to_state_;

  // The page that was showing before ShowSearchResults(true) was invoked.
  int page_before_search_ = 0;

  // Manages the pagination for the launcher pages.
  PaginationModel pagination_model_;

  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_
