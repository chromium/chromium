// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class IconButton;
class CloseButton;
class FocusModeChipCarousel;
class SystemTextfield;

// The class will be used in the `FocusModeDetailedView` under the task view
// container to let the user create, edit, select, or deselect a task for a
// focus session.
class ASH_EXPORT FocusModeTaskView : public views::BoxLayoutView {
 public:
  FocusModeTaskView();
  FocusModeTaskView(const FocusModeTaskView&) = delete;
  FocusModeTaskView& operator=(const FocusModeTaskView&) = delete;
  ~FocusModeTaskView() override;

  // Handles finished editing event from the text field, updates `task_title_`,
  // and saves the task information to the focus mode controller and user
  // prefs.
  // TODO(b/305085993): Update task data representation once API is integrated.
  void SelectTask(const std::u16string& task_title);

 private:
  friend class FocusModeTaskViewTest;

  class TaskTextfieldController;

  // TODO(b/306272008): Check off or deselect a task
  void OnRadioButtonPressed() {}
  void OnDeselectButtonPressed() {}

  // If `show_selected_state` is true, it means that there is a task selected
  // by the user for a focus session, then we will show `radio_button_` and
  // `deselect_button_`, update the style of `textfield_`, and hide the
  // selection carousel; otherwise, we will hide the two buttons, update the
  // style of `textfield_`, show the carousel, and let the user to create a new
  // task, edit an existing task, or select a task from the carousel.
  void UpdateStyle(bool show_selected_state);

  // TODO(b/306272008): Update the image of `radio_button_` to a check icon if
  // it was clicked by the user.
  raw_ptr<IconButton> radio_button_;
  raw_ptr<CloseButton> deselect_button_;

  // Title of the selected task.
  std::u16string task_title_;
  raw_ptr<SystemTextfield> textfield_ = nullptr;
  std::unique_ptr<TaskTextfieldController> textfield_controller_;
  raw_ptr<FocusModeChipCarousel> chip_carousel_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_VIEW_H_
