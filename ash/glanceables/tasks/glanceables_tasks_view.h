// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_

#include <memory>

#include "ash/api/tasks/tasks_types.h"
#include "ash/ash_export.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class LabelButton;
}  // namespace views

namespace ash {

class Combobox;
class GlanceablesListFooterView;
class GlanceablesProgressBarView;
class GlanceablesTaskView;
class TasksComboboxModel;

// Temporary interface to allow smooth migration from `TasksBubbleView` to
// `GlanceablesTasksView`.
class ASH_EXPORT GlanceablesTasksViewBase : public GlanceableTrayChildBubble {
 public:
  explicit GlanceablesTasksViewBase(DetailedViewDelegate* delegate);
  GlanceablesTasksViewBase(const GlanceablesTasksViewBase&) = delete;
  GlanceablesTasksViewBase& operator=(const GlanceablesTasksViewBase&) = delete;
  ~GlanceablesTasksViewBase() override = default;

  // Invalidates any pending tasks, or tasks lists requests. Called when the
  // glanceables bubble widget starts closing to avoid unnecessary UI updates.
  virtual void CancelUpdates() = 0;
};

// Glanceables view responsible for interacting with Google Tasks.
class ASH_EXPORT GlanceablesTasksView : public GlanceablesTasksViewBase,
                                        public views::ViewObserver {
 public:
  METADATA_HEADER(GlanceablesTasksView);

  GlanceablesTasksView(DetailedViewDelegate* delegate,
                       ui::ListModel<api::TaskList>* task_list);
  GlanceablesTasksView(const GlanceablesTasksView&) = delete;
  GlanceablesTasksView& operator=(const GlanceablesTasksView&) = delete;
  ~GlanceablesTasksView() override;

  // GlanceablesTasksViewBase:
  void CancelUpdates() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* view) override;

 private:
  // Handles press behavior for the header icon in `tasks_header_view_` and the
  // "See all" button in `list_footer_view_`.
  void ActionButtonPressed(TasksLaunchSource source);

  // Handles press behavior for `add_new_task_button_`.
  void AddNewTaskButtonPressed();

  // Creates a `GlanceablesTaskView` instance with bound callbacks.
  std::unique_ptr<GlanceablesTaskView> CreateTaskView(
      const std::string& task_list_id,
      const api::Task* task);

  // Handles switching between tasks lists.
  void SelectedTasksListChanged();
  void ScheduleUpdateTasksList(bool initial_update);
  void UpdateTasksList(const std::string& task_list_id,
                       const std::string& task_list_title,
                       bool initial_update,
                       ui::ListModel<api::Task>* tasks);

  // Announces text describing the task list state through a screen
  // reader, using `task_list_combo_box_view_` view accessibility helper.
  void AnnounceListStateOnComboBoxAccessibility();

  // Marks the specified task as completed.
  void MarkTaskAsCompleted(const std::string& task_list_id,
                           const std::string& task_id,
                           bool completed);

  // Saves the task (either creates or updates the existing one).
  void SaveTask(const std::string& task_list_id,
                const std::string& task_id,
                const std::string& title);

  // Model for the combobox used to change the active task list.
  std::unique_ptr<TasksComboboxModel> tasks_combobox_model_;

  // Tracks the number of tasks show. Used for sizing.
  int num_tasks_shown_ = 0;
  int num_tasks_ = 0;

  // The number of times that the tasks list has been changed during the
  // lifetime of this view.
  int tasks_list_change_count_ = 0;

  // Whether the first task list has been shown during the lifetime of this
  // view.
  bool first_task_list_shown_ = false;

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> tasks_header_view_ = nullptr;
  raw_ptr<Combobox, ExperimentalAsh> task_list_combo_box_view_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> button_container_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> task_items_container_view_ = nullptr;
  raw_ptr<views::LabelButton, ExperimentalAsh> add_new_task_button_ = nullptr;
  raw_ptr<GlanceablesListFooterView, ExperimentalAsh> list_footer_view_ =
      nullptr;
  raw_ptr<GlanceablesProgressBarView, ExperimentalAsh> progress_bar_ = nullptr;

  // Records the time when the bubble was about to request a task list. Used for
  // metrics.
  base::TimeTicks tasks_requested_time_;

  base::ScopedObservation<views::View, views::ViewObserver>
      combobox_view_observation_{this};

  base::WeakPtrFactory<GlanceablesTasksView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_VIEW_H_
