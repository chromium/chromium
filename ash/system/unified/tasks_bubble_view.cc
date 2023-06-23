// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/tasks_bubble_view.h"

#include <algorithm>

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kMaximumTasks = 5;
constexpr int kGlanceablesTaskHeaderHeight = 32;
constexpr int kGlanceablesTaskViewHeight = 48;
constexpr int kGlanceablesTaskFooterHeight = 24;
constexpr int kGlanceablesFooterMargin = 12;
constexpr int kGlanceableTaskVerticalMargin = 2;

}  // namespace

namespace ash {

TasksBubbleView::TasksBubbleView(DetailedViewDelegate* delegate)
    : GlanceableTrayChildBubble(delegate) {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      kGlanceablesVerticalMargin / 2, kGlanceablesLeftRightMargin / 2)));

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
  const int width = kRevampedTrayMenuWidth - 2 * kGlanceablesLeftRightMargin;
  const int height =
      kGlanceablesVerticalMargin * 3 + kGlanceablesTaskHeaderHeight +
      kGlanceablesTaskFooterHeight + kGlanceablesFooterMargin * 2 +
      kGlanceablesTaskViewHeight * std::max(num_tasks_shown_, 1);
  return gfx::Size(width, height);
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
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));

  task_items_container_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  task_items_container_view_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStart);
  task_items_container_view_->SetMainAxisAlignment(
      views::LayoutAlignment::kStart);
  task_items_container_view_->SetOrientation(
      views::LayoutOrientation::kVertical);
  task_items_container_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(2));
  task_items_container_view_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kGlanceablesVerticalMargin, 0,
                                                 kGlanceablesFooterMargin, 0)));
  task_items_container_view_->SetInteriorMargin(
      gfx::Insets::TLBR(kGlanceableTaskVerticalMargin, 0, 0, 0));

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

  tasks_footer_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_footer_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_footer_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  tasks_footer_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_footer_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));

  tasks_bubble_details_ =
      tasks_footer_view_->AddChildView(std::make_unique<views::Label>());
  tasks_bubble_details_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Views should not be individually selected for accessibility. Accessible
  // name and behavior comes from the parent.
  tasks_bubble_details_->GetViewAccessibility().OverrideIsIgnored(true);
  tasks_bubble_details_->SetBackgroundColor(SK_ColorTRANSPARENT);
  tasks_bubble_details_->SetAutoColorReadabilityEnabled(false);

  // Create a transparent separator to push `action_button_` to the right-most
  // corner of the tasks_header_view_.
  separator_ =
      tasks_footer_view_->AddChildView(std::make_unique<views::View>());
  separator_->SetPreferredSize((gfx::Size(kRevampedTrayMenuWidth, 1)));
  separator_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(2));

  action_button_ =
      tasks_footer_view_->AddChildView(std::make_unique<IconButton>(
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
  num_tasks_shown_ = 0;
  int num_tasks_ = 0;
  for (const auto& task : *tasks) {
    if (task->completed) {
      continue;
    }

    if (num_tasks_shown_ < kMaximumTasks) {
      auto* view = task_items_container_view_->AddChildView(
          std::make_unique<GlanceablesTaskView>(task_list_id, task.get()));
      view->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
      view->SetOrientation(views::LayoutOrientation::kHorizontal);
      ++num_tasks_shown_;
    }
    ++num_tasks_;
  }

  tasks_bubble_details_->SetText(
      l10n_util::GetStringFUTF16(IDS_GLANCEABLES_TASKS_DETAILS_FOOTER,
                                 base::NumberToString16(num_tasks_shown_),
                                 base::NumberToString16(num_tasks_)));
}

BEGIN_METADATA(TasksBubbleView, views::View)
END_METADATA

}  // namespace ash
