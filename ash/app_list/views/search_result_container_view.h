// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

// SearchResultContainerView is a base class for views that contain multiple
// search results. SearchPageView holds these in a list and manages which one is
// selected. There can be one result within one SearchResultContainerView
// selected at a time; moving off the end of one container view selects the
// first element of the next container view, and vice versa
class ASH_EXPORT SearchResultContainerView : public views::View,
                                             public views::ViewObserver,
                                             public ui::ListModelObserver {
  METADATA_HEADER(SearchResultContainerView, views::View)

 public:
  class Delegate {
   public:
    // Called whenever results in the container start changing, i.e. during
    // ScheduleUpdate(). It will be followed up with
    // OnSearchResultContainerResultsChanged() when the update completes.
    virtual void OnSearchResultContainerResultsChanging() = 0;

    // Called whenever results in the container change, i.e. during |Update()|.
    virtual void OnSearchResultContainerResultsChanged() = 0;
  };
  explicit SearchResultContainerView(AppListViewDelegate* view_delegate);

  SearchResultContainerView(const SearchResultContainerView&) = delete;
  SearchResultContainerView& operator=(const SearchResultContainerView&) =
      delete;

  ~SearchResultContainerView() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Sets the search results to listen to.
  void SetResults(SearchModel::SearchResults* results);
  SearchModel::SearchResults* results() { return results_; }

  size_t num_results() const { return num_results_; }

  virtual SearchResultBaseView* GetResultViewAt(size_t index) = 0;

  // Activates or deactivates results container - when not active, the container
  // will not react to search model updates. Generally, container will be active
  // when search is in progress in the launcher UI that owns the results
  // container.
  // Setting the container as inactive will not clear results, so results remain
  // shown during search results UI hide animation. To clear the container when
  // the search UI gets hidden, call `ResetAndHide()`.
  void SetActive(bool active);

  // Clears all results in the container, and hides the container view.
  void ResetAndHide();

  // Information needed to configure search result visibility animations when
  // result updates are animated.
  struct ResultsAnimationInfo {
    // Total number of visible views (either title or result views).
    int total_views = 0;

    // Total number of visible result views.
    int total_result_views = 0;

    // The index of the first result view that should be animated.
    int first_animated_result_view_index = 0;

    // The number of views that are animating (either title or result views).
    int animating_views = 0;

    // Whether fast search result update animations should be used.
    bool use_short_animations = false;
  };

  // Information needed to determine if a search result shuold have an updated
  // animation.
  struct SearchResultAimationMetadata {
    // The ID of the search result.
    std::string result_id;

    // Whether animations should be skipped for this search result.
    bool skip_animations = false;
  };

  // Schedules animations for result list updates. Expected to be implemented
  // for search result containers that animate result updates.
  // `aggregate_animation_info` The aggregated animation information for all
  // search result containers that appear in the search results UI before this
  // container.
  // Returns the animation info for this container.
  virtual std::optional<ResultsAnimationInfo> ScheduleResultAnimations(
      const ResultsAnimationInfo& aggregate_animation_info);

  // Appends search result IDs of the search results shown by the container
  // view into 'result_ids_'
  virtual void AppendShownResultMetadata(
      std::vector<SearchResultAimationMetadata>* result_metadata_);

  // Returns whether the container view has any animating child views.
  virtual bool HasAnimatingChildView();

  bool horizontally_traversable() const { return horizontally_traversable_; }

  // Allows a container to define its traversal behavior
  void set_horizontally_traversable(bool horizontally_traversable) {
    horizontally_traversable_ = horizontally_traversable;
  }

  // Called when the result selection controller updates its selected result.
  virtual void OnSelectedResultChanged();

  // Batching method that actually performs the update and updates layout.
  void Update();

  // Returns whether an update is currently scheduled for this container.
  bool UpdateScheduled();

  // Functions to allow derivative classes to add/remove observed result views.
  void AddObservedResultView(SearchResultBaseView* result_view);
  void RemoveObservedResultView(SearchResultBaseView* result_view);

  // Overridden from ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override;
  void ListItemsRemoved(size_t start, size_t count) override;
  void ListItemMoved(size_t index, size_t target_index) override;
  void ListItemsChanged(size_t start, size_t count) override;

  // Returns the first result in the container view. Returns nullptr if it does
  // not exist.
  SearchResultBaseView* GetFirstResultView();

  AppListViewDelegate* view_delegate() const { return view_delegate_; }

  // Runs scheduled update for the view. Returns whether the update was
  // actually run (i.e. whether an update was scheduled).
  bool RunScheduledUpdateForTest();

 protected:
  // Fades the view in and animates a vertical transform based on the view's
  // position in the overall search container view.
  static void ShowViewWithAnimation(views::View* result_view,
                                    int position,
                                    bool use_short_animations);

  // Updates the visibility of this container view and its children result
  // views. Force hiding all views if `force_hide` is set to true.
  virtual void UpdateResultsVisibility(bool force_hide) = 0;

  // Returns the title label if there is one, otherwise returns nullptr.
  virtual views::View* GetTitleLabel() = 0;

  // Returns the views in the container that will be animated to become visible.
  virtual std::vector<views::View*> GetViewsToAnimate() = 0;

  // A search result list view may be disabled if there are fewer search result
  // categories than there are search result list views in the
  // 'productivity_launcher_search_view_'. A disabled view does not query the
  // search model.
  bool enabled_ = true;

 private:
  // Schedules an Update call using |update_factory_|. Do nothing if there is a
  // pending call.
  void ScheduleUpdate();

  // Updates UI with model. Returns the number of visible results.
  virtual int DoUpdate() = 0;

  raw_ptr<Delegate> delegate_ = nullptr;

  size_t num_results_ = 0;

  // If true, left/right key events will traverse this container
  bool horizontally_traversable_ = false;

  raw_ptr<SearchModel::SearchResults> results_ =
      nullptr;  // Owned by SearchModel.

  // view delegate for notifications.
  const raw_ptr<AppListViewDelegate> view_delegate_;

  // Whether the container is observing search result model, and updating when
  // results in the model change.
  bool active_ = false;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      result_view_observations_{this};

  // The factory that consolidates multiple Update calls into one.
  base::WeakPtrFactory<SearchResultContainerView> update_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
