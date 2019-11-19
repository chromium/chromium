// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class AppListViewDelegate;
class SearchResultBaseView;
class ViewShadow;

// The search results page for the app list.
class APP_LIST_EXPORT SearchResultPageView
    : public AppListPage,
      public SearchResultContainerView::Delegate {
 public:
  explicit SearchResultPageView(AppListViewDelegate* view_delegate);
  ~SearchResultPageView() override;

  void AddSearchResultContainerView(
      SearchModel::SearchResults* result_model,
      SearchResultContainerView* result_container);

  const std::vector<SearchResultContainerView*>& result_container_views() {
    return result_container_views_;
  }

  bool IsFirstResultTile() const;
  bool IsFirstResultHighlighted() const;

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // AppListPage overrides:
  void OnHidden() override;
  void OnShown() override;

  void OnAnimationStarted(ash::AppListState from_state,
                          ash::AppListState to_state) override;
  void OnAnimationUpdated(double progress,
                          ash::AppListState from_state,
                          ash::AppListState to_state) override;
  gfx::Size GetPreferredSearchBoxSize() const override;
  base::Optional<int> GetSearchBoxTop(
      ash::AppListViewState view_state) const override;
  gfx::Rect GetPageBoundsForState(
      ash::AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

  // Overridden from SearchResultContainerView::Delegate :
  void OnSearchResultContainerResultsChanging() override;
  void OnSearchResultContainerResultsChanged() override;
  void OnSearchResultContainerResultFocused(
      SearchResultBaseView* focused_result_view) override;

  void OnAssistantPrivacyInfoViewCloseButtonPressed();

  views::View* contents_view() { return contents_view_; }

  SearchResultBaseView* first_result_view() const { return first_result_view_; }
  ResultSelectionController* result_selection_controller() {
    return result_selection_controller_.get();
  }

 private:
  // Separator between SearchResultContainerView.
  class HorizontalSeparator;

  // Sort the result container views.
  void ReorderSearchResultContainers();

  // Passed to |result_selection_controller_| as a callback that gets called
  // when the currently selected result changes.
  // Ensures that |scroller_| visible rect contains the newly selected result.
  void SelectedResultChanged();

  AppListViewDelegate* view_delegate_;

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  // |ResultSelectionController| handles selection within the
  // |result_container_views_|
  std::unique_ptr<ResultSelectionController> result_selection_controller_;

  std::vector<HorizontalSeparator*> separators_;

  // View containing SearchCardView instances. Owned by view hierarchy.
  views::View* const contents_view_;

  // The first search result's view or nullptr if there's no search result.
  SearchResultBaseView* first_result_view_ = nullptr;

  views::View* assistant_privacy_info_view_ = nullptr;

  std::unique_ptr<ash::ViewShadow> view_shadow_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
