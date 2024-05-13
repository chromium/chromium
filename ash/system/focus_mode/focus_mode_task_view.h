// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class FocusModeChipCarousel;

// The class will be used in the `FocusModeDetailedView` under the task view
// container to let the user create, edit, select, or deselect a task for a
// focus session.
class ASH_EXPORT FocusModeTaskView : public views::BoxLayoutView {
  METADATA_HEADER(FocusModeTaskView, views::BoxLayoutView)

 public:
  FocusModeTaskView();
  FocusModeTaskView(const FocusModeTaskView&) = delete;
  FocusModeTaskView& operator=(const FocusModeTaskView&) = delete;
  ~FocusModeTaskView() override;

  // Handles finished editing event from the text field, creates, saves, and
  // selects a new task with the user entered task title.
  // TODO(b/306271332): Create a new task.
  void AddOrUpdateTask(const std::u16string& task_title);

  // Updates `task_title_` and saves the task information to the focus mode
  // controller.
  void OnTaskSelected(const FocusModeTask& task_entry);

  // Clears the stored task data, and fetches an updated task list to display in
  // the carousel.
  void OnClearTask();

  views::ImageButton* radio_button_for_testing() { return radio_button_; }
  views::ImageButton* deselect_button_for_testing() { return deselect_button_; }
  FocusModeChipCarousel* chip_carousel_for_testing() { return chip_carousel_; }

 private:
  class TaskTextfield;
  class TaskTextfieldController;

  // Called when the active state of `textfield_` changes to update the focus
  // ring and update the style.
  void PaintFocusRingAndUpdateStyle();

  // Called when `radio_button_` is pressed to mark a task as completed.
  void OnCompleteTask();

  // Called when `deselect_button_` is pressed to remove a selected task.
  void OnDeselectButtonPressed();

  // Called when `add_task_button_` is pressed to focus on `textfield_`.
  void OnAddTaskButtonPressed();

  // Called when tasks have been fetched from the tasks provider.
  void OnTasksFetched(const std::vector<FocusModeTask>& tasks);

  // If `show_selected_state` is true, it means that there is a task selected
  // by the user for a focus session, then we will show `radio_button_` and
  // `deselect_button_`, update the style of `textfield_`, and hide the
  // selection carousel; otherwise, we will hide the two buttons, update the
  // style of `textfield_`, show the carousel, and let the user to create a new
  // task, edit an existing task, or select a task from the carousel.
  void UpdateStyle(bool show_selected_state);

  // TODO(b/306272008): Update the image of `radio_button_` to a check icon if
  // it was clicked by the user.
  raw_ptr<views::ImageButton> radio_button_ = nullptr;
  raw_ptr<views::ImageButton> deselect_button_ = nullptr;
  // Shows up on the left side of `textfield_` when there is no selected task.
  raw_ptr<views::ImageButton> add_task_button_ = nullptr;

  // Contains a `radio_button_`, a `deselect_button_`, an `add_task_button_` and
  // a `textfield_`.
  raw_ptr<views::BoxLayoutView> textfield_container_ = nullptr;

  // Title of the selected task.
  std::u16string task_title_;
  raw_ptr<TaskTextfield> textfield_ = nullptr;
  std::unique_ptr<TaskTextfieldController> textfield_controller_;
  raw_ptr<FocusModeChipCarousel> chip_carousel_ = nullptr;

  base::WeakPtrFactory<FocusModeTaskView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
