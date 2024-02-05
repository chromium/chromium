// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/view.h"

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
  METADATA_HEADER(ContinueTaskContainerView, views::View)

 public:
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

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  void Update();

  size_t num_results() const { return num_results_; }
  size_t num_file_results() const { return num_file_results_; }
  size_t num_desks_admin_template_results() const {
    return num_desks_admin_template_results_;
  }

  void SetResults(SearchModel::SearchResults* results);

  // See AppsGridView::DisableFocusForShowingActiveFolder().
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Start the animation for showing the suggestions in the continue section.
  // Suggestion views will slide in from an evenly distributed amount of
  // `available_space` into their final positions.
  // This animation must only be used in clamshell mode.
  void AnimateSlideInSuggestions(int available_space,
                                 base::TimeDelta duration,
                                 gfx::Tween::Type tween);

  base::OneShotTimer* animations_timer_for_test() { return &animations_timer_; }

 private:
  void ScheduleUpdate();

  // Sets the `view` to be ignored by the continue task container layout
  // manager, and disables the view.
  void RemoveViewFromLayout(ContinueTaskView* view);

  // Lays out children in a single row centered in the container. Number of
  // items displayed will depend on available space. This will not enforce any
  // number of `columns_`.
  void InitializeTabletLayout();

  // Lays out children in a table with a specific number of `columns_`. This
  // displays views to stretch as to use all vertical space available in the
  // container. Extra views are added in multiple rows.
  void InitializeClamshellLayout();

  // Describes how old task views should animate when the set of tasks shown in
  // the container updates.
  enum class TaskViewRemovalAnimation {
    // The task remained in the same position - the view does not animate.
    kNone,
    // The task got removed - the view should fade out.
    kFadeOut,
    // The associated result has moved within the container, and will slide and
    // fade out from the current position while the new result view slides and
    // fades in).
    kSlideOut,
  };

  // Determines how a task view that's shown in the continue task container
  // before a task list update should animate when the list of tasks changes.
  // `old_index` - the task index in task list from before container update.
  // `new_task_ids` - the list of task that will be shown in the container after
  // container update.
  TaskViewRemovalAnimation GetRemovalAnimationForTaskView(
      ContinueTaskView* task_view,
      size_t old_index,
      const std::vector<std::string>& new_task_ids);

  // Schedules animation for updating list of results shown in the task
  // container.
  // `views_to_fade_out` - Set of views whose associated results got removed,
  // and that should be faded out. These views will be removed when the
  // animation completes.
  // `views_to_slide_out` - Views whose associated results moved to another
  // index within the task container, and which should slide out of the task
  // container bounds (while the new result view slides in). Views are mapped by
  // the associated result ID. These views will be removed when the animation
  // completes.
  // `views_remaining_in_place` - Views whose associated results remained at the
  // same index within the tack container. These views will not animate, and
  // will be replaced by new views immediately. These views will be removed when
  // the animation completes.
  void ScheduleContainerUpdateAnimation(
      const std::set<views::View*>& views_to_fade_out,
      const std::map<std::string, views::View*>& views_to_slide_out,
      const std::map<std::string, views::View*>& views_remaining_in_place);

  // Aborts all in-progress tasks update animations.
  void AbortTasksUpdateAnimations();

  // Removes all child views that have been kept around just for container
  // update animation.
  void ClearAnimatingViews();

  // Moves focus up by one row, or up-and-out of the section.
  void MoveFocusUp();

  // Moves focus down by one row, or down-and-out of the section.
  void MoveFocusDown();

  // Returns the index in `suggestion_tasks_views_` of the currently focused
  // task view, or -1 if no task view is focused.
  int GetIndexOfFocusedTaskView() const;

  const raw_ptr<AppListViewDelegate> view_delegate_;

  // A callback to be invoked after an Update request finishes.
  OnResultsChanged update_callback_;
  raw_ptr<SearchModel::SearchResults> results_ =
      nullptr;  // Owned by SearchModel.

  // The list of tasks views for the container.
  std::vector<raw_ptr<ContinueTaskView, VectorExperimental>>
      suggestion_tasks_views_;

  // The number of results shown in the container. Each result has one view.
  size_t num_results_ = 0;

  // The number of file results shown in the container - different from
  // num_results_ when release notes result is shown in the container. Release
  // note result does not count towards min number of items needed to show
  // continue section, so `num_files_results_` should be used to determine
  // whether continue section can be shown.
  size_t num_file_results_ = 0;

  // The number of admin templates will be shown in the continue section.
  size_t num_desks_admin_template_results_ = 0;

  // The number of columns available for the view. This is ignored in tablet
  // mode.
  int columns_ = 0;

  // Whether or not the view is showing for a table mode launcher or not.
  bool tablet_mode_ = false;

  // Set of results that need to animate out of the task container when the set
  // of results shown in the container gets updated. The views are only still
  // needed for the update animation and should be removed once the animation
  // completes.
  std::vector<raw_ptr<ContinueTaskView, VectorExperimental>>
      views_to_remove_after_animation_;

  // Timer which when active disables container update animations. The timer
  // gets started when the container gets shown. The goal is to disable update
  // animations after the container gets first shown until the initial set of
  // results stabilizes.
  base::OneShotTimer animations_timer_;

  base::ScopedObservation<SearchModel::SearchResults, ui::ListModelObserver>
      list_model_observation_{this};

  base::WeakPtrFactory<ContinueTaskContainerView> update_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_TASK_CONTAINER_VIEW_H_
