// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/scoped_observation.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/views/view.h"

namespace views {
class FlexLayout;
class TableLayout;
}  // namespace views

namespace ash {

class AppListViewDelegate;
class ContinueTaskView;

// The container for the Continue Tasks results view. The view contains a preset
// number of ContinueTaskViews that get populated based on the list of results
// passed in SetResult.
// ContinueTaskContainerView will accommodate Continue Task views in a grid-like
// layout with the number of columns specified at construction.
class ASH_EXPORT ContinueTaskContainerView : public ui::ListModelObserver,
                                             public views::View {
 public:
  METADATA_HEADER(ContinueTaskContainerView);

  using OnResultsChanged = base::RepeatingClosure;

  ContinueTaskContainerView(AppListViewDelegate* view_delegate,
                            int columns,
                            OnResultsChanged update_callback,
                            bool tablet_mode);
  ContinueTaskContainerView(const ContinueTaskContainerView&) = delete;
  ContinueTaskContainerView& operator=(const ContinueTaskContainerView&) =
      delete;

  ~ContinueTaskContainerView() override;

  // ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override;
  void ListItemsRemoved(size_t start, size_t count) override;
  void ListItemMoved(size_t index, size_t target_index) override;
  void ListItemsChanged(size_t start, size_t count) override;

  void Update();
  size_t num_results() const { return num_results_; }

  void SetResults(SearchModel::SearchResults* results);

  // See AppsGridView::DisableFocusForShowingActiveFolder().
  void DisableFocusForShowingActiveFolder(bool disabled);

 private:
  void ScheduleUpdate();

  // Initializes the view's layout manager to use |flex_layout_|. FlexLayout is
  // used in tablet mode only. Views will be laid out in a single row centered
  // in the container. Number of items displayed will depend on available space.
  // This will not enforce any number of `columns_`.
  void InitializeFlexLayout();

  // Initializes the view's layout manager to use |table_layout_|. TableLayout
  // is used in clamshell mode only. Views are laid out in a table with a
  // specific number of `columns_`. This displays views to stretch as to use
  // all vertical space available in the container. Extra views are added in
  // multiple rows.
  void InitializeTableLayout();

  AppListViewDelegate* const view_delegate_;
  // A callback to be invoked after an Update request finishes.
  OnResultsChanged update_callback_;
  SearchModel::SearchResults* results_ = nullptr;  // Owned by SearchModel.

  // Only one of the layouts is to be set.
  // `flex_layout_`  aligns the views as a single row centered in the container.
  // Used in tablet mode.
  views::FlexLayout* flex_layout_ = nullptr;
  // `table_layout_`  aligns the views as a table with multiple rows stretched
  // to fill the container. Used in clamshell mode.
  views::TableLayout* table_layout_ = nullptr;

  // The list of tasks views for the container.
  std::vector<ContinueTaskView*> suggestion_tasks_views_;
  // The number of results shown on the container. Each result has one view.
  size_t num_results_ = 0;

  // The number of columns available for the view. This is ignored in tablet
  // mode.
  int columns_ = 0;

  // Whether or not the view is showing for a table mode launcher or not.
  bool tablet_mode_ = false;

  base::ScopedObservation<SearchModel::SearchResults, ui::ListModelObserver>
      list_model_observation_{this};

  base::WeakPtrFactory<ContinueTaskContainerView> update_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_
