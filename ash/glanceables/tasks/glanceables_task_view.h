// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

// GlanceablesTaskView uses `views::FlexLayout` to show tasks metadata within
// the TasksBubbleView.
// +---------------------------------------------------------------+
// |`GlanceablesTaskView`                                          |
// |                                                               |
// | +-----------------+ +---------------------------------------+ |
// | |'button_'        | |'contents_view_'                       | |
// | |                 | | +---------------------------------  + | |
// | |                 | | |'tasks_title_view_'                | | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | +---------------------------------  + | |
// | |                 | | |'tasks_details_view_'              | | |
// | |                 | | +-----------------------------------+ | |
// | +-----------------+ +---------------------------------------+ |
// +---------------------------------------------------------------+
class ASH_EXPORT GlanceablesTaskView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesTaskView);

  GlanceablesTaskView(const std::string& task_list_id,
                      const GlanceablesTask* task);
  GlanceablesTaskView(const GlanceablesTaskView&) = delete;
  GlanceablesTaskView& operator=(const GlanceablesTaskView&) = delete;
  ~GlanceablesTaskView() override;

  void ButtonPressed();

  const views::ImageButton* GetButtonForTest() const;
  bool GetCompletedForTest() const;

 private:
  class CheckButton;

  void SetupTasksLabel(bool completed);

  // Owned by views hierarchy.
  raw_ptr<CheckButton> button_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> contents_view_ = nullptr;
  raw_ptr<views::BoxLayoutView, ExperimentalAsh> tasks_title_view_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> tasks_details_view_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> tasks_label_ = nullptr;

  // ID for the task list that owns this task.
  const std::string task_list_id_;
  // ID for the task represented by this view.
  const std::string task_id_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
