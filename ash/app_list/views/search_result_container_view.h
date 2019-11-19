// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_

#include <stddef.h>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

// SearchResultContainerView is a base class for views that contain multiple
// search results. SearchPageView holds these in a list and manages which one is
// selected. There can be one result within one SearchResultContainerView
// selected at a time; moving off the end of one container view selects the
// first element of the next container view, and vice versa
class APP_LIST_EXPORT SearchResultContainerView : public views::View,
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

    // Called whenever a result within the container gains focus.
    virtual void OnSearchResultContainerResultFocused(
        SearchResultBaseView* focused_result_view) = 0;
  };
  explicit SearchResultContainerView(AppListViewDelegate* view_delegate);
  ~SearchResultContainerView() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Sets the search results to listen to.
  void SetResults(SearchModel::SearchResults* results);
  SearchModel::SearchResults* results() { return results_; }

  int num_results() const { return num_results_; }

  virtual SearchResultBaseView* GetResultViewAt(size_t index) = 0;

  bool horizontally_traversable() const { return horizontally_traversable_; }

  // Allows a container to define its traversal behavior
  void set_horizontally_traversable(bool horizontally_traversable) {
    horizontally_traversable_ = horizontally_traversable;
  }

  void set_container_score(double score) { container_score_ = score; }
  double container_score() const { return container_score_; }

  // Updates the distance_from_origin() properties of the results in this
  // container. |y_index| is the absolute y-index of the first result of this
  // container (counting from the top of the app list).
  virtual void NotifyFirstResultYIndex(int y_index);

  // Gets the number of down keystrokes from the beginning to the end of this
  // container.
  virtual int GetYSize();

  // Batching method that actually performs the update and updates layout.
  void Update();

  // Returns whether an update is currently scheduled for this container.
  bool UpdateScheduled();

  // Overridden from views::View:
  const char* GetClassName() const override;

  // Overridden from views::ViewObserver:
  void OnViewFocused(View* observed_view) override;

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
  virtual SearchResultBaseView* GetFirstResultView();

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

  double container_score_ = 0.0;

  SearchModel::SearchResults* results_ = nullptr;  // Owned by SearchModel.

  // view delegate for notifications.
  bool shown_ = false;
  AppListViewDelegate* const view_delegate_;

  ScopedObserver<views::View, views::ViewObserver> result_view_observer_{this};

  // The factory that consolidates multiple Update calls into one.
  base::WeakPtrFactory<SearchResultContainerView> update_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchResultContainerView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
