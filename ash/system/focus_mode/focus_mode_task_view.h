// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class IconButton;
class CloseButton;
class SystemTextfield;

// The class will be used in the `FocusModeDetailedView` under the task view
// container to let the user create, edit, select, or deselect a task for a
// focus session.
class ASH_EXPORT FocusModeTaskView : public views::FlexLayoutView {
 public:
  FocusModeTaskView();
  FocusModeTaskView(const FocusModeTaskView&) = delete;
  FocusModeTaskView& operator=(const FocusModeTaskView&) = delete;
  ~FocusModeTaskView() override;

 private:
  class TaskTextfieldController;

  // Handles finished editing event from the text field, updates `task_title_`.
  void OnFinishedEditing();

  void OnRadioButtonPressed() {}
  void OnDeselectButtonPressed() {}

  // If `show_selected_state` is true, it means that there is a task selected by
  // the user for a focus session, then we will show `radio_button_` and
  // `deselect_button_` and update the style of `textfield_`; otherwise, we will
  // hide the two buttons and also update the style of `textfield_` and let the
  // user to create a new task, or edit an existing task.
  void UpdateTextfieldStyle(bool show_selected_state);

  // TODO(b/306272008): Update the image of `radio_button_` to a check icon if
  // it was clicked by the user.
  raw_ptr<IconButton> radio_button_;
  raw_ptr<CloseButton> deselect_button_;

  // Title of the task.
  std::u16string task_title_;
  raw_ptr<SystemTextfield> textfield_ = nullptr;
  std::unique_ptr<TaskTextfieldController> textfield_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
