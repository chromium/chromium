// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class BoxLayoutView;
class ImageButton;
}  // namespace views

namespace ash {

struct GlanceablesTask;

// GlanceablesTaskView uses `views::FlexLayout` to show tasks metadata within
// the TasksBubbleView.
// +---------------------------------------------------------------+
// |`GlanceablesTaskView`                                          |
// |                                                               |
// | +-----------------+ +---------------------------------------+ |
// | |'button_'        | |'contents_view_'                       | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_title_view_'                | | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_details_view_'              | | |
// | |                 | | +-----------------------------------+ | |
// | +-----------------+ +---------------------------------------+ |
// +---------------------------------------------------------------+
class ASH_EXPORT GlanceablesTaskView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesTaskView);

  // Modes of `tasks_title_view_` (simple label or text field).
  enum class TaskTitleViewState { kView, kEdit };

  GlanceablesTaskView(const std::string& task_list_id,
                      const GlanceablesTask* task);
  GlanceablesTaskView(const GlanceablesTaskView&) = delete;
  GlanceablesTaskView& operator=(const GlanceablesTaskView&) = delete;
  ~GlanceablesTaskView() override;

  const views::ImageButton* GetButtonForTest() const;
  bool GetCompletedForTest() const;

 private:
  class CheckButton;
  class TaskTitleButton;

  // Handles press events on `button_`.
  void CheckButtonPressed();

  // Handles press events on `task_title_button_`.
  void TaskTitleButtonPressed();

  // Handles finished editing event from the text field, updates `task_title_`
  // and propagates new `title` to the server.
  void OnFinishedEditing(const std::u16string& title);

  // Updates `tasks_title_view_` according to `state`.
  void UpdateTaskTitleViewForState(TaskTitleViewState state);

  // Owned by views hierarchy.
  raw_ptr<CheckButton> button_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> contents_view_ = nullptr;
  raw_ptr<views::BoxLayoutView, ExperimentalAsh> tasks_title_view_ = nullptr;
  raw_ptr<TaskTitleButton, ExperimentalAsh> task_title_button_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> tasks_details_view_ = nullptr;

  // ID for the task list that owns this task.
  const std::string task_list_id_;

  // ID for the task represented by this view.
  const std::string task_id_;

  // Title of the task.
  std::u16string task_title_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
