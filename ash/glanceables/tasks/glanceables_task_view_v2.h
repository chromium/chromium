// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_V2_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_V2_H_

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
class LabelButton;
}  // namespace views

namespace ash {

namespace api {
struct Task;
}  // namespace api

// GlanceablesTaskViewV2 uses `views::FlexLayout` to show tasks metadata within
// the `GlanceablesTasksView` or `TasksBubbleView`.
// +---------------------------------------------------------------+
// |`GlanceablesTaskViewV2`                                        |
// |                                                               |
// | +-----------------+ +---------------------------------------+ |
// | |'check_button_'  | |'contents_view_'                       | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_title_view_'                | | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_details_view_'              | | |
// | |                 | | +-----------------------------------+ | |
// | +-----------------+ +---------------------------------------+ |
// +---------------------------------------------------------------+
class ASH_EXPORT GlanceablesTaskViewV2 : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesTaskViewV2);

  using MarkAsCompletedCallback =
      base::RepeatingCallback<void(const std::string& task_id, bool completed)>;
  using SaveCallback = base::RepeatingCallback<void(
      base::WeakPtr<GlanceablesTaskViewV2> view,
      const std::string& task_id,
      const std::string& title,
      api::TasksClient::OnTaskSavedCallback callback)>;

  // Modes of `tasks_title_view_` (simple label or text field).
  enum class TaskTitleViewState { kNotInitialized, kView, kEdit };

  GlanceablesTaskViewV2(const api::Task* task,
                        MarkAsCompletedCallback mark_as_completed_callback,
                        SaveCallback save_callback,
                        base::RepeatingClosure edit_in_browser_callback);
  GlanceablesTaskViewV2(const GlanceablesTaskViewV2&) = delete;
  GlanceablesTaskViewV2& operator=(const GlanceablesTaskViewV2&) = delete;
  ~GlanceablesTaskViewV2() override;

  const views::ImageButton* GetCheckButtonForTest() const;
  bool GetCompletedForTest() const;

  // Updates `tasks_title_view_` according to `state`.
  void UpdateTaskTitleViewForState(TaskTitleViewState state);

 private:
  class CheckButton;
  class TaskTitleButton;

  // Handles press events on `check_button_`.
  void CheckButtonPressed();

  // Handles press events on `task_title_button_`.
  void TaskTitleButtonPressed();

  // Handles finished editing event from the text field, updates `task_title_`
  // and propagates new `title` to the server.
  void OnFinishedEditing(const std::u16string& title);

  // Handles completion of running `save_callback_` callback.
  // `task` - newly created or updated task.
  void OnSaved(const api::Task* task);

  // Owned by views hierarchy.
  raw_ptr<CheckButton> check_button_ = nullptr;
  raw_ptr<views::FlexLayoutView> contents_view_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_title_view_ = nullptr;
  raw_ptr<TaskTitleButton> task_title_button_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_details_view_ = nullptr;
  raw_ptr<views::LabelButton> edit_in_browser_button_ = nullptr;

  // ID for the task represented by this view.
  std::string task_id_;

  // Title of the task.
  std::u16string task_title_;

  // Marks the task as completed.
  const MarkAsCompletedCallback mark_as_completed_callback_;

  // Saves the task (either creates or updates the existing one).
  const SaveCallback save_callback_;

  // `edit_in_browser_button_` callback that opens the Tasks in browser.
  const base::RepeatingClosure edit_in_browser_callback_;

  base::WeakPtrFactory<GlanceablesTaskViewV2> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_V2_H_
