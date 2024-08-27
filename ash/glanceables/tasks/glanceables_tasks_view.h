// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_

#include <memory>
#include <optional>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_tasks_error_type.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

class GURL;

namespace views {
class LabelButton;
}  // namespace views

namespace ash {

class GlanceablesTasksComboboxModel;
class GlanceablesTaskView;

// Glanceables view responsible for interacting with Google Tasks.
class ASH_EXPORT GlanceablesTasksView
    : public GlanceablesTimeManagementBubbleView {
  METADATA_HEADER(GlanceablesTasksView, GlanceablesTimeManagementBubbleView)
 public:
  explicit GlanceablesTasksView(const ui::ListModel<api::TaskList>* task_lists);
  GlanceablesTasksView(const GlanceablesTasksView&) = delete;
  GlanceablesTasksView& operator=(const GlanceablesTasksView&) = delete;
  ~GlanceablesTasksView() override;

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // GlanceablesTimeManagementBubbleView:
  void AnimationEnded(const gfx::Animation* animation) override;

  // Invalidates any pending tasks, or tasks lists requests. Called when the
  // glanceables bubble widget starts closing to avoid unnecessary UI updates.
  void CancelUpdates();

  // Updates the cached task lists to `task_lists` and the tasks that are
  // supposed to show.
  void UpdateTaskLists(const ui::ListModel<api::TaskList>* task_lists);

  void EndResizeAnimationForTest();

 private:
  // The context of why the current task list is shown.
  enum class ListShownContext {
    // The list is a cached one that will be updated later after the lists data
    // are fetched.
    kCachedList,
    // The list that is loaded by default when users open glanceables.
    kInitialList,
    // The list is manually selected by the users through
    // `task_list_combo_box_view_`.
    kUserSelectedList
  };

  // GlanceablesTimeManagementBubbleView:
  void OnHeaderIconPressed() override;
  void OnFooterButtonPressed() override;
  void SelectedListChanged() override;
  void AnimateResize(ResizeAnimation::Type resize_type) override;

  // Handles press behavior for `add_new_task_button_`.
  void AddNewTaskButtonPressed();

  // Creates a `GlanceablesTaskView` instance with bound callbacks.
  std::unique_ptr<GlanceablesTaskView> CreateTaskView(
      const std::string& task_list_id,
      const api::Task* task);

  // Handles switching between tasks lists.
  void ScheduleUpdateTasks(ListShownContext context);
  void RetryUpdateTasks(ListShownContext context);
  void UpdateTasksInTaskList(
      const std::string& task_list_id,
      const std::string& task_list_title,
      ListShownContext context,
      bool fetch_success,
      std::optional<google_apis::ApiErrorCode> http_error,
      const ui::ListModel<api::Task>* tasks);

  // Called as a `state_change_callback` when a task view state changes between
  // "view" and "edit" state, which causes changes in the task view preferred
  // size. `view_expanding` indicates whether the task view is expanding or
  // collapsing in size.
  void HandleTaskViewStateChange(bool view_expanding);

  // Marks the specified task as completed.
  void MarkTaskAsCompleted(const std::string& task_list_id,
                           const std::string& task_id,
                           bool completed);

  // Handles press behavior for icons that are used to open Google Tasks in the
  // browser.
  void ActionButtonPressed(TasksLaunchSource source, const GURL& target_url);

  // Saves the task (either creates or updates the existing one).
  // `view`     - individual task view which triggered this request.
  // `callback` - done callback passed from an individual task view.
  void SaveTask(const std::string& task_list_id,
                base::WeakPtr<GlanceablesTaskView> view,
                const std::string& task_id,
                const std::string& title,
                api::TasksClient::OnTaskSavedCallback callback);

  // Handles completion of `SaveTask`.
  // `view`     - individual task view which triggered this request.
  // `callback` - callback passed from an individual task view via `SaveTask`.
  // `task`     - newly created or edited task if the request completes
  //              successfully, `nullptr` otherwise.
  void OnTaskSaved(base::WeakPtr<GlanceablesTaskView> view,
                   const std::string& task_id,
                   api::TasksClient::OnTaskSavedCallback callback,
                   google_apis::ApiErrorCode http_error,
                   const api::Task* task);

  // Returns the current showing task list.
  const api::TaskList* GetActiveTaskList() const;

  // Creates and shows `error_message_` that depends on the `error_type` and
  // `button_action_type`.
  void ShowErrorMessageWithType(
      GlanceablesTasksErrorType error_type,
      ErrorMessageToast::ButtonActionType button_action_type);

  // Returns the string to show on `error_message_` according to the
  // `error_type`.
  std::u16string GetErrorString(GlanceablesTasksErrorType error_type) const;

  // Removes `task_view` from the tasks container.
  void RemoveTaskView(base::WeakPtr<GlanceablesTaskView> task_view);

  // This function should be called with `is_loading` = true if `this` is
  // waiting for fetched data to be returned. After the data arrives, resets the
  // states by calling with `is_loading` = false.
  void SetIsLoading(bool is_loading);

  // Animates visibility updates for a task view. It assumes that at most one
  // task view changes visibility at the time - currently, this is exclusively
  // used for task view added to the list in response to the user clicking the
  // button to add a new task.
  // When hiding/removing the task view, a copy of the task view layer will
  // be created for the animation, so the view can be hidden/removed
  // immediately.
  void AnimateTaskViewVisibility(views::View* task, bool visible);
  void OnTaskViewAnimationCompleted();

  // Caching `combobox_model_` from GlanceablesTimeManagementBubbleView.
  raw_ptr<GlanceablesTasksComboboxModel> tasks_combobox_model_;

  // The number of times that the tasks list has been changed during the
  // lifetime of this view.
  int tasks_list_change_count_ = 0;

  // Whether the first task list has been shown during the lifetime of this
  // view.
  bool first_task_list_shown_ = false;

  // Owned by views hierarchy.
  raw_ptr<views::LabelButton> add_new_task_button_ = nullptr;

  // An invisible view added at the last element to the task list container to
  // more easily track the offset of the bottom of the list from the target
  // position mid task view state change animations. The transform will be used
  // to synchronize `resize_animation_` with task view resize / task list layout
  // animations - if the sentinel, i.e. the bottom of the task list is
  // animating, the bubble preferred size will be "offset" by the sentinel
  // transform so the bottom of the bubble tracks the bottom of the task list
  // (otherwise, if the animations get slightly out of sync, animating task
  // views may appear to jitter as their bottom padding changes slightly).
  raw_ptr<views::View> task_list_sentinel_ = nullptr;

  // Copy of a task view whose visibility is animating to "hidden". The copy
  // of the layer is used so the original view can be removed from the view
  // hierarchy immediately.
  std::unique_ptr<ui::Layer> animating_task_view_layer_;

  // The type of resize animation that is currently running.
  std::optional<ResizeAnimation::Type> running_resize_animation_ = std::nullopt;

  // Records the time when the bubble was about to request a task list. Used for
  // metrics.
  base::TimeTicks tasks_requested_time_;

  // Cached to reset the value of the index of `task_list_combo_box_view_` when
  // the target task list failed to be loaded.
  std::optional<size_t> cached_selected_list_index_ = std::nullopt;

  // Number of tasks added by the user for the currently selected task list.
  // Task is considered "added" if task creation was requested via tasks API.
  // The count is reset when the selected task list changes.
  int added_tasks_ = 0;

  // Whether the current task list was empty when it got selected.
  bool task_list_initially_empty_ = false;

  // Whether the user had a single task list with no tasks when the current task
  // list was selected.
  bool user_with_no_tasks_ = false;

  // Time stamp of when the view was created.
  const base::Time shown_time_;

  // The model containing task views shown within the tasks bubble. Used to set
  // up task view animations in response to a task view change. When animating
  // task views, their transform is calculated relative to expected view bounds
  // after an imminent layout. The reference/target bounds for the animation
  // are cached as view ideal bounds in the model, so the transform can be
  // recalculated if another task view state changes before the task list view
  // layout gets updated (e.g. if the same user action causes once view to
  // transition to kEdit, and another one to kView state).
  views::ViewModelT<views::View> task_view_model_;

  // Callback that recreates `task_list_combo_box_view_`.
  base::OnceClosure recreate_combobox_callback_;

  base::WeakPtrFactory<GlanceablesTasksView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
