// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_

#include <memory>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_tasks_error_type.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class GURL;

namespace views {
class LabelButton;
}  // namespace views

namespace ash {

class Combobox;
class CounterExpandButton;
class GlanceablesListFooterView;
class GlanceablesProgressBarView;
class GlanceablesTasksComboboxModel;
class GlanceablesTaskView;

// Glanceables view responsible for interacting with Google Tasks.
class ASH_EXPORT GlanceablesTasksView
    : public GlanceablesTimeManagementBubbleView,
      public views::ViewObserver {
  METADATA_HEADER(GlanceablesTasksView, GlanceablesTimeManagementBubbleView)

 public:
  explicit GlanceablesTasksView(const ui::ListModel<api::TaskList>* task_lists);
  GlanceablesTasksView(const GlanceablesTasksView&) = delete;
  GlanceablesTasksView& operator=(const GlanceablesTasksView&) = delete;
  ~GlanceablesTasksView() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* view) override;

  // Invalidates any pending tasks, or tasks lists requests. Called when the
  // glanceables bubble widget starts closing to avoid unnecessary UI updates.
  void CancelUpdates();

  // Updates the cached task lists to `task_lists` and the tasks that are
  // supposed to show.
  void UpdateTaskLists(const ui::ListModel<api::TaskList>* task_lists);

  // Creates `this` view's own background and updates layout accordingly.
  void CreateElevatedBackground();

  void SetExpandState(bool is_expanded);
  bool is_expanded() const { return is_expanded_; }

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

  // Toggles `is_expanded_` and updates the layout.
  void ToggleExpandState();

  // Handles press behavior for `add_new_task_button_`.
  void AddNewTaskButtonPressed();

  // Creates a `GlanceablesTaskView` instance with bound callbacks.
  std::unique_ptr<GlanceablesTaskView> CreateTaskView(
      const std::string& task_list_id,
      const api::Task* task);

  // Handles switching between tasks lists.
  void SelectedTasksListChanged();
  void ScheduleUpdateTasks(ListShownContext context);
  void RetryUpdateTasks(ListShownContext context);
  void UpdateTasksInTaskList(const std::string& task_list_id,
                             const std::string& task_list_title,
                             ListShownContext context,
                             bool fetch_success,
                             const ui::ListModel<api::Task>* tasks);

  // Announces text describing the task list state through a screen
  // reader, using `task_list_combo_box_view_` view accessibility helper.
  void AnnounceListStateOnComboBoxAccessibility();

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
                   const api::Task* task);

  // Returns the current showing task list.
  const api::TaskList* GetActiveTaskList() const;

  // Creates and shows `error_message_` that depends on the `error_type` and
  // `button_action_type`.
  void ShowErrorMessageWithType(
      GlanceablesTasksErrorType error_type,
      GlanceablesErrorMessageView::ButtonActionType button_action_type);

  // Returns the string to show on `error_message_` according to the
  // `error_type`.
  std::u16string GetErrorString(GlanceablesTasksErrorType error_type) const;

  // Removes `task_view` from the tasks container.
  void RemoveTaskView(base::WeakPtr<GlanceablesTaskView> task_view);

  // Creates and initializes `task_list_combo_box_view_`.
  void CreateComboBoxView();

  // This function should be called with `is_loading` = true if `this` is
  // waiting for fetched data to be returned. After the data arrives, resets the
  // states by calling with `is_loading` = false.
  void SetIsLoading(bool is_loading);

  // Model for the combobox used to change the active task list.
  std::unique_ptr<GlanceablesTasksComboboxModel> tasks_combobox_model_;

  // The number of times that the tasks list has been changed during the
  // lifetime of this view.
  int tasks_list_change_count_ = 0;

  // Whether the first task list has been shown during the lifetime of this
  // view.
  bool first_task_list_shown_ = false;

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> tasks_header_view_ = nullptr;
  // This is a simple label that copies the label style on
  // `task_list_combo_box_view_` so that it can visually replace it when
  // `task_list_combo_box_view_` is hidden.
  raw_ptr<views::Label> combobox_replacement_label_ = nullptr;
  raw_ptr<Combobox> task_list_combo_box_view_ = nullptr;
  raw_ptr<views::ScrollView> content_scroll_view_ = nullptr;
  raw_ptr<views::View> task_items_container_view_ = nullptr;
  raw_ptr<views::LabelButton> add_new_task_button_ = nullptr;
  raw_ptr<GlanceablesListFooterView> list_footer_view_ = nullptr;
  raw_ptr<GlanceablesProgressBarView> progress_bar_ = nullptr;
  raw_ptr<CounterExpandButton> expand_button_ = nullptr;

  // Whether the view is expanded and showing the contents in
  // `content_scroll_view_`.
  bool is_expanded_ = true;

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

  // Callback that recreates `task_list_combo_box_view_`.
  base::OnceClosure recreate_combobox_callback_;

  base::ScopedObservation<views::View, views::ViewObserver>
      combobox_view_observation_{this};

  base::WeakPtrFactory<GlanceablesTasksView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
