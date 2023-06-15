// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/tasks_bubble_view.h"

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace ash {

TasksBubbleView::TasksBubbleView() {
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetOrientation(views::LayoutOrientation::kVertical);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      kGlanceablesVerticalMargin, kGlanceablesLeftRightMargin)));

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
  // TODO(b:277268122): Scale height based on `task_items_container_view_`
  // contents.
  return gfx::Size(kRevampedTrayMenuWidth - 2 * kGlanceablesLeftRightMargin,
                   kGlanceableMinHeight - 2 * kGlanceablesVerticalMargin);
}

void TasksBubbleView::InitViews(ui::ListModel<GlanceablesTaskList>* task_list) {
  // TODO(b:277268122): Implement empty tasks glanceable state.
  if (task_list->item_count() == 0) {
    return;
  }

  tasks_header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_header_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  tasks_header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  tasks_header_view_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(kGlanceablesVerticalMargin, 0)));

  task_items_container_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  task_items_container_view_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStart);
  task_items_container_view_->SetMainAxisAlignment(
      views::LayoutAlignment::kStart);
  task_items_container_view_->SetOrientation(
      views::LayoutOrientation::kVertical);

  task_icon_view_ =
      tasks_header_view_->AddChildView(std::make_unique<views::ImageView>());

  tasks_combobox_model_ = std::make_unique<TasksComboboxModel>(task_list);
  task_list_combo_box_view_ = tasks_header_view_->AddChildView(
      std::make_unique<views::Combobox>(tasks_combobox_model_.get()));
  task_list_combo_box_view_->SetSizeToLargestLabel(false);

  // TODO(b:277268122): Implement accessibility behavior.
  task_list_combo_box_view_->SetTooltipTextAndAccessibleName(
      u"Task list selector");
  task_list_combo_box_view_->SetCallback(base::BindRepeating(
      &TasksBubbleView::SelectedTasksListChanged, base::Unretained(this)));
  task_list_combo_box_view_->SetSelectedIndex(0);

  // Create a transparent separator to push `action_button_` to the right-most
  // corner of the tasks_header_view_.
  separator_ =
      tasks_header_view_->AddChildView(std::make_unique<views::View>());
  separator_->SetPreferredSize((gfx::Size(kRevampedTrayMenuWidth, 1)));
  separator_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(2));

  action_button_ =
      tasks_header_view_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&TasksBubbleView::ActionButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating, &vector_icons::kLaunchIcon,
          u"Open tasks app", /*is_togglable=*/false,
          /*has_border=*/false));

  ScheduleUpdateTasksList();
}

void TasksBubbleView::ActionButtonPressed() {
  // TODO(b:277268122): launch tasks web app.
}

void TasksBubbleView::SelectedTasksListChanged() {
  task_items_container_view_->RemoveAllChildViews();
  ScheduleUpdateTasksList();
}

void TasksBubbleView::ScheduleUpdateTasksList() {
  if (!task_list_combo_box_view_->GetSelectedIndex().has_value()) {
    return;
  }

  GlanceablesTaskList* active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  ash::Shell::Get()->glanceables_v2_controller()->GetTasksClient()->GetTasks(
      active_task_list->id,
      base::BindOnce(&TasksBubbleView::UpdateTasksList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id));
}

void TasksBubbleView::UpdateTasksList(const std::string& task_list_id,
                                      ui::ListModel<GlanceablesTask>* tasks) {
  for (const auto& task : *tasks) {
    if (!task->completed) {
      auto* view = task_items_container_view_->AddChildView(
          std::make_unique<GlanceablesTaskView>(task_list_id, task.get()));
      view->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
      view->SetOrientation(views::LayoutOrientation::kHorizontal);
    }
  }
}

BEGIN_METADATA(TasksBubbleView, views::View)
END_METADATA

}  // namespace ash
