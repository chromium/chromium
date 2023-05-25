// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TASKS_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TASKS_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class Combobox;
class ImageButton;
class ImageView;
}  // namespace views

namespace ash {

// 'contents_view_' uses nested `FlexLayoutView`s to layout the tasks bubble.
// configurations.
// +---------------------------------------------------------------+
// |`contents_view_`                                               |
// | +-----------------------------------------------------------+ |
// | |tasks_header_view_                                         | |
// | +-----------------------------------------------------------+ |
// | +-----------------------------------------------------------+ |
// | |task_items_list_view_                                      | |
// | +-----------------------------------------------------------+ |
// +---------------------------------------------------------------+
//
// TODO(b:277268122): Override action_button_ class.
// +----------------------------------------------------------------+
// |tasks_header_view_                                              |
// |+---------------+ +-------------------------+ +--------------+  |
// ||task_icon_view_| |task_list_combo_box_view_| |action_button_|  |
// |+---------------+ +-------------------------+ +--------------+  |
// +----------------------------------------------------------------+
//
// TODO(b:277268122): Add TaskItemListView and TaskCheckboxView classes.
// +----------------------------------------------------------------+
// |task_items_list_view_                                           |
// | +----------------------------------------------------------- + |
// | |TaskCheckboxView                                            | |
// | +----------------------------------------------------------- + |
// | +----------------------------------------------------------- + |
// | |TaskCheckboxView                                            | |
// | +----------------------------------------------------------- + |
// +----------------------------------------------------------------+

class ASH_EXPORT TasksBubbleView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(TasksBubbleView);

  TasksBubbleView();
  TasksBubbleView(const TasksBubbleView&) = delete;
  TasksBubbleView& operator=(const TasksBubbleView&) = delete;
  ~TasksBubbleView() override;

  bool IsMenuRunning();

  views::Combobox* task_list_combo_box_view() const {
    return task_list_combo_box_view_;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class DateTrayTest;
  // Setup child views.
  void InitViews(ui::ListModel<GlanceablesTaskList>* task_list);

  // Handles on-click behavior for `action_button_`
  void ActionButtonPressed();

  // Handles switching between tasks lists.
  void SelectedTasksListChanged();

  raw_ptr<views::FlexLayoutView, ExperimentalAsh> tasks_header_view_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::ImageView, ExperimentalAsh> task_icon_view_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::Combobox, ExperimentalAsh> task_list_combo_box_view_ =
      nullptr;  // Owned by views hierarchy.
  // TODO(b:277268122): replace stand-in button.
  raw_ptr<views::ImageButton, ExperimentalAsh> action_button_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> task_items_list_view_ =
      nullptr;  // Owned by views hierarchy.

  base::WeakPtrFactory<TasksBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TASKS_BUBBLE_VIEW_H_
