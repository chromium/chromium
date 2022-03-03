// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_

#include <stddef.h>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
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

  int num_results() const { return num_results_; }

  virtual SearchResultBaseView* GetResultViewAt(size_t index) = 0;

  // Information needed to configure search result visibility animations when
  // result updates are animated.
  struct ResultsAnimationInfo {
    // Total number of visible views (either title or result views).
    int total_views = 0;

    // The number of views that are animating (either title or result views).
    int animating_views = 0;

    // Whether fast search result update animations should be used.
    bool use_short_animations = false;
  };

  // Schedules animations for result list updates. Expected to be implemented
  // for search result containers that animate result updates.
  // `aggregate_animation_info` The aggregated animation information for all
  // search result containers that appear in the search results UI before this
  // container.
  // Returns the animation info for this container.
  virtual absl::optional<ResultsAnimationInfo> ScheduleResultAnimations(
      const ResultsAnimationInfo& aggregate_animation_info);

  // Returns whether the container view has any animating child views.
  virtual bool HasAnimatingChildView();

  bool horizontally_traversable() const { return horizontally_traversable_; }

  // Allows a container to define its traversal behavior
  void set_horizontally_traversable(bool horizontally_traversable) {
    horizontally_traversable_ = horizontally_traversable;
  }

  // Batching method that actually performs the update and updates layout.
  void Update();

  // Returns whether an update is currently scheduled for this container.
  bool UpdateScheduled();

  // Overridden from views::View:
  const char* GetClassName() const override;

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

  // Called from SearchResultPageView OnShown/OnHidden
  void SetShown(bool shown);
  bool shown() const { return shown_; }
  // Called when SetShowing has changed a result.
  virtual void OnShownChanged();

  AppListViewDelegate* view_delegate() const { return view_delegate_; }

 private:
  // Schedules an Update call using |update_factory_|. Do nothing if there is a
  // pending call.
  void ScheduleUpdate();

  // Updates UI with model. Returns the number of visible results.
  virtual int DoUpdate() = 0;

  Delegate* delegate_ = nullptr;

  int num_results_ = 0;

  // If true, left/right key events will traverse this container
  bool horizontally_traversable_ = false;

  SearchModel::SearchResults* results_ = nullptr;  // Owned by SearchModel.

  // view delegate for notifications.
  bool shown_ = false;
  AppListViewDelegate* const view_delegate_;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      result_view_observations_{this};

  // The factory that consolidates multiple Update calls into one.
  base::WeakPtrFactory<SearchResultContainerView> update_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
