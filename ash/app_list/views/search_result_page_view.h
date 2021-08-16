// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_box_model_observer.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace views {
class DialogDelegateView;
class Textfield;
}

namespace ash {

class AppListMainView;
class PrivacyContainerView;
class SearchResultBaseView;
class SearchResultListView;
class SearchResultTileItemListView;
class SearchResultPageAnchoredDialog;
class ViewShadow;

// The search results page for the app list.
class ASH_EXPORT SearchResultPageView
    : public AppListPage,
      public SearchResultContainerView::Delegate,
      public SearchBoxModelObserver {
 public:
  explicit SearchResultPageView(SearchModel* search_model);
  ~SearchResultPageView() override;

  void InitializeContainers(AppListViewDelegate* view_delegate,
                            AppListMainView* app_list_main_view,
                            views::Textfield* search_box);

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

  // AppListPage overrides:
  void OnWillBeHidden() override;
  void OnHidden() override;
  void OnShown() override;
  void AnimateYPosition(AppListViewState target_view_state,
                        const TransformAnimator& animator,
                        float default_offset) override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity,
                                 bool restore_opacity) override;
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
  absl::optional<int> GetSearchBoxTop(
      AppListViewState view_state) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

  // Overridden from SearchResultContainerView::Delegate:
  void OnSearchResultContainerResultsChanging() override;
  void OnSearchResultContainerResultsChanged() override;

  // Overridden from SearchBoxModelObserver:
  void Update() override;
  void SearchEngineChanged() override;
  void ShowAssistantChanged() override;

  // Shows a dialog widget, and anchors it within the search results page. The
  // dialog will be positioned relative to the search box bounds, and will be
  // repositioned as the page layout changes. The dialog will be closed if the
  // search results page gets hidden.
  // |dialog| should not yet have a widget.
  void ShowAnchoredDialog(std::unique_ptr<views::DialogDelegateView> dialog);

  views::View* contents_view() { return contents_view_; }

  SearchResultBaseView* first_result_view() const { return first_result_view_; }
  ResultSelectionController* result_selection_controller() {
    return result_selection_controller_.get();
  }

  SearchResultPageAnchoredDialog* anchored_dialog_for_test() {
    return anchored_dialog_.get();
  }

  // Returns background color for the given state.
  SkColor GetBackgroundColorForState(AppListState state) const;

  PrivacyContainerView* GetPrivacyContainerViewForTest();
  SearchResultTileItemListView* GetSearchResultTileItemListViewForTest();
  SearchResultListView* GetSearchResultListViewForTest();

 private:
  // Separator between SearchResultContainerView.
  class HorizontalSeparator;

  // Sets visibility of result container and separator views so only containers
  // that contain some results are shown.
  void UpdateResultContainersVisibility();

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

  // Called when the widget anchored in the search results page gets closed.
  void OnAnchoredDialogClosed();

  template <typename T>
  T* AddSearchResultContainerView(std::unique_ptr<T> result_container) {
    auto* result = result_container.get();
    AddSearchResultContainerViewInternal(std::move(result_container));
    return result;
  }

  void AddSearchResultContainerViewInternal(
      std::unique_ptr<SearchResultContainerView> result_container);

  // The search model for which the results are displayed.
  SearchModel* const search_model_;

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  // |ResultSelectionController| handles selection within the
  // |result_container_views_|
  std::unique_ptr<ResultSelectionController> result_selection_controller_;

  // Search result containers shown within search results page (and added to
  // `result_container_views_`).
  PrivacyContainerView* privacy_container_view_ = nullptr;
  SearchResultTileItemListView* search_result_tile_item_list_view_ = nullptr;
  SearchResultListView* search_result_list_view_ = nullptr;

  // Separator view shown between search result tile item list and search
  // results list.
  HorizontalSeparator* result_lists_separator_ = nullptr;

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

  std::unique_ptr<ViewShadow> view_shadow_;

  // The dialog anchored within the search results page.
  std::unique_ptr<SearchResultPageAnchoredDialog> anchored_dialog_;

  base::ScopedObservation<SearchBoxModel, SearchBoxModelObserver>
      search_box_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
