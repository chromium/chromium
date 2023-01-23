// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include <memory>

#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/ash_export.h"

namespace ash {

class AppListViewDelegate;
class AppListSearchView;
class SearchBoxView;
class SearchResultPageAnchoredDialog;
class SystemShadow;

// The search results page for the app list.
class ASH_EXPORT SearchResultPageView : public AppListPage {
 public:
  SearchResultPageView();

  SearchResultPageView(const SearchResultPageView&) = delete;
  SearchResultPageView& operator=(const SearchResultPageView&) = delete;

  ~SearchResultPageView() override;

  void InitializeContainers(AppListViewDelegate* view_delegate,
                            SearchBoxView* search_box_view);

  // Overridden from views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;

  // AppListPage overrides:
  void OnHidden() override;
  void OnShown() override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity) override;
  void UpdatePageBoundsForState(AppListState state,
                                const gfx::Rect& contents_bounds,
                                const gfx::Rect& search_box_bounds) override;
  gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  void OnAnimationStarted(AppListState from_state,
                          AppListState to_state) override;
  gfx::Size GetPreferredSearchBoxSize() const override;

  // Whether any results are available for selection within the search result
  // UI.
  bool CanSelectSearchResults() const;

  AppListSearchView* search_view_for_test() { return search_view_; }

  SearchResultPageAnchoredDialog* dialog_for_test() {
    return dialog_controller_->dialog();
  }

  // Hide zero state search result view when ProductivityLauncher is enabled.
  bool ShouldShowSearchResultView() const;

  // Called when the app list search query changes and new search is about to
  // start or cleared.
  void UpdateForNewSearch();

  // Sets visibility of result container and separator views so only containers
  // that contain some results are shown.
  void UpdateResultContainersVisibility();

 private:
  // All possible states for the search results page. Used with productivity
  // launcher.
  enum class SearchResultsState { kClosed, kActive, kExpanded };

  // Animates from the current search results state to the `target_state`. Used
  // with productivity launcher.
  void AnimateToSearchResultsState(SearchResultsState target_state);

  // Transitions between `from_rect` and `to_rect` by animating the clip rect.
  void AnimateBetweenBounds(const gfx::Rect& from_rect,
                            const gfx::Rect& to_rect);

  // Called when the clip rect animation between bounds has ended.
  void OnAnimationBetweenBoundsEnded();

  // Get the page bounds according to the input SearchResultsState.
  gfx::Rect GetPageBoundsForResultState(SearchResultsState state) const;

  // Get the corner radius associated with the SearchResultsState.
  int GetCornerRadiusForSearchResultsState(SearchResultsState state);

  // Search result container used for productivity launcher.
  AppListSearchView* search_view_ = nullptr;

  // View containing SearchCardView instances. Owned by view hierarchy.
  views::View* root_view_ = nullptr;

  // The currently shown search results state. Used with productivity launcher.
  SearchResultsState current_search_results_state_ =
      SearchResultsState::kClosed;

  std::unique_ptr<SystemShadow> shadow_;

  // The controller that manages dialogs modal to the search results page.
  std::unique_ptr<SearchResultPageDialogController> dialog_controller_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
