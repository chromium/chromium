// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_tasks_model.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class FocusModeChipCarousel;
class SystemTextfield;

// The class will be used in the `FocusModeDetailedView` under the task view
// container to let the user create, edit, select, or deselect a task for a
// focus session.
class ASH_EXPORT FocusModeTaskView : public views::BoxLayoutView,
                                     public FocusModeTasksModel::Observer {
  METADATA_HEADER(FocusModeTaskView, views::BoxLayoutView)

 public:
  explicit FocusModeTaskView(bool is_network_connected);
  FocusModeTaskView(const FocusModeTaskView&) = delete;
  FocusModeTaskView& operator=(const FocusModeTaskView&) = delete;
  ~FocusModeTaskView() override;

  // FocusModeTasksModel::Observer:
  void OnSelectedTaskChanged(const std::optional<FocusModeTask>& task) override;
  void OnTasksUpdated(const std::vector<FocusModeTask>& tasks) override;
  void OnTaskCompleted(const FocusModeTask& task) override;

  // Sets `task_entry` as the currently selected task.
  void OnTaskSelectedFromCarousel(const FocusModeTask& task_entry);

  // Clears the selected task if we have one. Forwards this to the model.
  void OnClearTask();

  views::ImageButton* complete_button_for_testing() { return complete_button_; }
  views::ImageButton* deselect_button_for_testing() { return deselect_button_; }
  FocusModeChipCarousel* chip_carousel_for_testing() { return chip_carousel_; }
  SystemTextfield* GetTaskTextfieldForTesting();
  views::BoxLayoutView* textfield_container_for_testing() {
    return textfield_container_;
  }

  void CommitTextfieldContents(const std::u16string& contents);

 private:
  class TaskTextfield;
  class TaskTextfieldController;

  // Handles finished editing event from the text field, creates, saves, and
  // selects a new task with the user entered task title.
  void AddOrUpdateTask(const std::optional<TaskId>& task_id,
                       const std::u16string& task_title);

  // Called when the active state of `textfield_` changes to update the focus
  // ring and update the style.
  void PaintFocusRingAndUpdateStyle();

  // Called when `complete_button_` is pressed to mark a task as completed.
  // `update` is used to determine if we need to update the tasks provider (i.e.
  // we don't if the task is already marked as completed).
  void OnCompleteTask();

  // Called when `deselect_button_` is pressed to remove a selected task.
  void OnDeselectButtonPressed();

  // Called when `add_task_button_` is pressed to focus on `textfield_`.
  void OnAddTaskButtonPressed();

  // If `show_selected_state` is true, it means that there is a task selected
  // by the user for a focus session, then we will show `complete_button_` and
  // `deselect_button_`, update the style of `textfield_`, and hide the
  // selection carousel; otherwise, we will hide the two buttons, update the
  // style of `textfield_`, show the carousel, and let the user to create a new
  // task, edit an existing task, or select a task from the carousel. If
  // `is_network_connected` is false, we will show a different color for the
  // button and disable it as well.
  void UpdateStyle(bool show_selected_state, bool is_network_connected);

  // TODO(b/306272008): Update the image of `complete_button_` to a check icon
  // if it was clicked by the user.
  raw_ptr<views::ImageButton> complete_button_ = nullptr;
  raw_ptr<views::ImageButton> deselect_button_ = nullptr;
  // Shows up on the left side of `textfield_` when there is no selected task.
  raw_ptr<views::ImageButton> add_task_button_ = nullptr;

  // Contains a `complete_button_`, a `deselect_button_`, an `add_task_button_`
  // and a `textfield_`.
  raw_ptr<views::BoxLayoutView> textfield_container_ = nullptr;

  const bool is_network_connected_;

  // The id of the task being edited in the textfield. nullopt if it is a new
  // task.
  std::optional<TaskId> task_id_;

  raw_ptr<TaskTextfield> textfield_ = nullptr;
  std::unique_ptr<TaskTextfieldController> textfield_controller_;
  raw_ptr<FocusModeChipCarousel> chip_carousel_ = nullptr;

  // True while the completed task animation is running. Used to ignore the
  // selected task change for the UI.
  bool complete_animation_running_ = false;

  base::ScopedObservation<FocusModeTasksModel, FocusModeTasksModel::Observer>
      tasks_observation_{this};
  base::WeakPtrFactory<FocusModeTaskView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
