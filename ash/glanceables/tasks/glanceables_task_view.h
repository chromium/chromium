// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_

#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

namespace api {
struct Task;
}  // namespace api

// GlanceablesTaskView uses `views::FlexLayout` to show tasks metadata within
// the `GlanceablesTasksView` or `TasksBubbleView`.
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
  METADATA_HEADER(GlanceablesTaskView, views::FlexLayoutView)

 public:
  using MarkAsCompletedCallback =
      base::RepeatingCallback<void(const std::string& task_id, bool completed)>;

  // Modes of `tasks_title_view_` (simple label or text field).
  // TODO(b/315188389): Remove this as editing functions is replaced by
  // GlanceablesTaskViewV2 when the stable launch is enabled.
  enum class TaskTitleViewState { kView, kEdit };

  GlanceablesTaskView(const api::Task* task,
                      MarkAsCompletedCallback mark_as_completed_callback);
  GlanceablesTaskView(const GlanceablesTaskView&) = delete;
  GlanceablesTaskView& operator=(const GlanceablesTaskView&) = delete;
  ~GlanceablesTaskView() override;

  const views::ImageButton* GetButtonForTest() const;
  bool GetCompletedForTest() const;

  // Updates `tasks_title_view_` according to `state`.
  void UpdateTaskTitleViewForState(TaskTitleViewState state);

 private:
  class CheckButton;
  class TaskTitleButton;

  // Handles press events on `button_`.
  void CheckButtonPressed();

  // Handles press events on `task_title_button_`.
  void TaskTitleButtonPressed();

  // Owned by views hierarchy.
  raw_ptr<CheckButton> button_ = nullptr;
  raw_ptr<views::FlexLayoutView> contents_view_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_title_view_ = nullptr;
  raw_ptr<TaskTitleButton> task_title_button_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_details_view_ = nullptr;

  // ID for the task represented by this view.
  std::string task_id_;

  // Title of the task.
  std::u16string task_title_;

  // Marks the task as completed.
  MarkAsCompletedCallback mark_as_completed_callback_;

  base::WeakPtrFactory<GlanceablesTaskView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
