// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/tasks_bubble_view.h"

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace ash {

TasksBubbleView::TasksBubbleView() {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetOrientation(views::LayoutOrientation::kVertical);

  if (ash::Shell::Get()->glanceables_v2_controller()->GetTasksClient()) {
    ash::Shell::Get()
        ->glanceables_v2_controller()
        ->GetTasksClient()
        ->GetTaskLists(base::BindOnce(&TasksBubbleView::InitViews,
                                      weak_ptr_factory_.GetWeakPtr()));
  }
}

TasksBubbleView::~TasksBubbleView() = default;

bool TasksBubbleView::IsMenuRunning() {
  return task_list_combo_box_view_ &&
         task_list_combo_box_view_->IsMenuRunning();
}

void TasksBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(b:277268122): Implement accessibility behavior.
  if (!GetVisible()) {
    return;
  }
  node_data->role = ax::mojom::Role::kListBox;
  node_data->SetName(u"Glanceables Bubble Task View Accessible Name");
}

gfx::Size TasksBubbleView::CalculatePreferredSize() const {
  // TODO(b:277268122): Scale height based on task_items_list_view_ contents.
  return gfx::Size(kRevampedTrayMenuWidth - 2 * kGlanceablesLeftRightMargin,
                   kTasksGlanceableMinHeight - 2 * kGlanceablesVerticalMargin);
}

void TasksBubbleView::InitViews(ui::ListModel<GlanceablesTaskList>* task_list) {
  // TODO(b:277268122): Implement empty tasks glanceable state.
  if (task_list->item_count() == 0) {
    return;
  }

  tasks_header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  tasks_header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  task_items_list_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  task_items_list_view_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  task_items_list_view_->SetOrientation(views::LayoutOrientation::kVertical);
  task_items_list_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  task_icon_view_ =
      tasks_header_view_->AddChildView(std::make_unique<views::ImageView>());
  task_list_combo_box_view_ =
      tasks_header_view_->AddChildView(std::make_unique<views::Combobox>(
          std::make_unique<TasksComboboxModel>(task_list)));
  // TODO(b:277268122): Implement accessibility behavior.
  task_list_combo_box_view_->SetTooltipTextAndAccessibleName(
      u"Task list selector");
  task_list_combo_box_view_->SetCallback(base::BindRepeating(
      &TasksBubbleView::SelectedTasksListChanged, base::Unretained(this)));
  task_list_combo_box_view_->SetSelectedIndex(0);

  action_button_ =
      tasks_header_view_->AddChildView(views::ImageButton::CreateIconButton(
          base::BindRepeating(&TasksBubbleView::ActionButtonPressed,
                              base::Unretained(this)),
          views::kLaunchIcon, u"Open tasks app"));
}

void TasksBubbleView::ActionButtonPressed() {
  // TODO(b:277268122): launch tasks web app.
}

void TasksBubbleView::SelectedTasksListChanged() {
  // TODO(b:277268122): Update task_items_list_view_.
}

BEGIN_METADATA(TasksBubbleView, views::View)
END_METADATA

}  // namespace ash
