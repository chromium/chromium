// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class AppListMainView;
class ProductivityLauncherSearchView;
class SearchBoxView;
class SearchResultBaseView;
class SearchResultListView;
class SearchResultPageAnchoredDialog;
class SystemShadow;

// The search results page for the app list.
class ASH_EXPORT SearchResultPageView
    : public AppListPage,
      public AppListModelProvider::Observer,
      public SearchResultContainerView::Delegate {
 public:
  SearchResultPageView();

  SearchResultPageView(const SearchResultPageView&) = delete;
  SearchResultPageView& operator=(const SearchResultPageView&) = delete;

  ~SearchResultPageView() override;

  void InitializeContainers(AppListViewDelegate* view_delegate,
                            AppListMainView* app_list_main_view,
                            SearchBoxView* search_box_view);

  const std::vector<SearchResultContainerView*>& result_container_views() {
    return result_container_views_;
  }

  bool IsFirstResultTile() const;
  bool IsFirstResultHighlighted() const;

  // Overridden from views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // AppListPage overrides:
  void OnHidden() override;
  void OnShown() override;
  void AnimateYPosition(AppListViewState target_view_state,
                        const TransformAnimator& animator,
                        float default_offset) override;
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
  void OnAnimationUpdated(double progress,
                          AppListState from_state,
                          AppListState to_state) override;
  gfx::Size GetPreferredSearchBoxSize() const override;

  // Overridden from AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Overridden from SearchResultContainerView::Delegate:
  void OnSearchResultContainerResultsChanging() override;
  void OnSearchResultContainerResultsChanged() override;

  // Whether any results are available for selection within the search result
  // UI.
  bool CanSelectSearchResults() const;

  views::View* contents_view() { return contents_view_; }

  SearchResultBaseView* first_result_view() const { return first_result_view_; }
  ResultSelectionController* result_selection_controller() {
    return result_selection_controller_.get();
  }

  ProductivityLauncherSearchView* productivity_launcher_search_view_for_test() {
    return productivity_launcher_search_view_;
  }

  SearchResultPageAnchoredDialog* dialog_for_test() {
    return dialog_controller_->dialog();
  }

  // Returns background color for the given state.
  SkColor GetBackgroundColorForState(AppListState state) const;

  // Hide zero state search result view when ProductivityLauncher is enabled.
  bool ShouldShowSearchResultView() const;

  // Called when the app list search query changes and new search is about to
  // start or cleared.
  void UpdateForNewSearch();

  // Sets visibility of result container and separator views so only containers
  // that contain some results are shown.
  void UpdateResultContainersVisibility();

  SearchResultListView* GetSearchResultListViewForTest();

 private:
  // All possible states for the search results page. Used with productivity
  // launcher.
  enum class SearchResultsState { kClosed, kActive, kExpanded };

  // Passed to |result_selection_controller_| as a callback that gets called
  // when the currently selected result changes.
  // Ensures that |scroller_| visible rect contains the newly selected result.
  void SelectedResultChanged();

  // Sets whether changes in search result containers should be hidden from the
  // accessibility framework.
  // This is set while search results are being updated to reduce noisy updates
  // sent to the accessibility framework while the search result containers are
  // being rebuilt.
  // The |ignore| value is reset in NotifyA11yResultsChanged(), at which time
  // accessibility framework is notified that the view value/selected children
  // have changed.
  void SetIgnoreResultChangesForA11y(bool ignore);

  // Schedules a call to |NotifyA11yResultsChanged|. Called from
  // OnSearchResultContainerResultsChanged() when all result containers have
  // finished changing. The goal of the delay is to reduce the noise if the set
  // of results for a query has not stabilized, or while the user is still
  // changing the query.
  void ScheduleResultsChangedA11yNotification();

  // Notifies the accessibility framework that the set of search results has
  // changed.
  // Note: This ensures that results changes are not being hidden from a11y
  // framework.
  void NotifyA11yResultsChanged();

  // If required, sends a kSelection a11y notification for the currently
  // selected search result view.
  void NotifySelectedResultChanged();

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

  template <typename T>
  T* AddSearchResultContainerView(std::unique_ptr<T> result_container) {
    auto* result = result_container.get();
    AddSearchResultContainerViewInternal(std::move(result_container));
    return result;
  }

  void AddSearchResultContainerViewInternal(
      std::unique_ptr<SearchResultContainerView> result_container);

  AppListViewDelegate* view_delegate_ = nullptr;

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  // |ResultSelectionController| handles selection within the
  // |result_container_views_|
  std::unique_ptr<ResultSelectionController> result_selection_controller_;

  // Search result containers shown within search results page (and added to
  // `result_container_views_`).
  SearchResultListView* search_result_list_view_ = nullptr;
  // Search result container used for productivity launcher.
  ProductivityLauncherSearchView* productivity_launcher_search_view_ = nullptr;

  // View containing SearchCardView instances. Owned by view hierarchy.
  views::View* const contents_view_;

  // The first search result's view or nullptr if there's no search result.
  SearchResultBaseView* first_result_view_ = nullptr;

  // Timer used to delay calls to NotifyA11yResultsChanged().
  base::OneShotTimer notify_a11y_results_changed_timer_;

  // Whether the changes in search result containers are being hidden from the
  // accessibility framework.
  bool ignore_result_changes_for_a11y_ = false;

  // The last reported number of search results shown within search result
  // containers.
  int last_search_result_count_ = 0;

  // The currently shown search results state. Used with productivity launcher.
  SearchResultsState current_search_results_state_ =
      SearchResultsState::kClosed;

  std::unique_ptr<SystemShadow> shadow_;

  // The controller that manages dialogs modal to the search results page.
  std::unique_ptr<SearchResultPageDialogController> dialog_controller_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
