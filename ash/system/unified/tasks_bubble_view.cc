// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/tasks_bubble_view.h"

#include <algorithm>
#include <memory>

#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

constexpr int kMaximumTasks = 5;
constexpr int kTasksIconRightPadding = 4;
constexpr int kTasksIconViewSize = 32;
constexpr int kSpacingAboveListContainerView = 16;
constexpr int kInteriorGlanceableBubbleMargin = 16;

constexpr char kTasksManagementPage[] =
    "https://calendar.google.com/calendar/u/0/r/week?opentasks=1";

}  // namespace

namespace ash {

TasksBubbleView::TasksBubbleView(DetailedViewDelegate* delegate)
    : GlanceableTrayChildBubble(delegate) {
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

void TasksBubbleView::InitViews(ui::ListModel<GlanceablesTaskList>* task_list) {
  // TODO(b:277268122): Implement empty tasks glanceable state.
  if (task_list->item_count() == 0) {
    return;
  }

  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager
      ->SetInteriorMargin(gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin))
      .SetOrientation(views::LayoutOrientation::kVertical);

  tasks_header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_header_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  tasks_header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));

  task_items_container_view_ = AddChildView(std::make_unique<views::View>());
  task_items_container_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingAboveListContainerView, 0, 0, 0));
  task_items_container_view_->SetPaintToLayer();
  task_items_container_view_->layer()->SetFillsBoundsOpaquely(false);
  task_items_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  auto* layout = task_items_container_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(2);

  task_icon_view_ =
      tasks_header_view_->AddChildView(std::make_unique<views::ImageView>());
  task_icon_view_->SetPreferredSize(
      gfx::Size(kTasksIconViewSize, kTasksIconViewSize));
  task_icon_view_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, kTasksIconViewSize / 2));
  if (chromeos::features::IsJellyEnabled()) {
    task_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        kGlanceablesTasksIcon, cros_tokens::kCrosSysOnSurface));
  } else {
    task_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        kGlanceablesTasksIcon, kColorAshTextColorPrimary));
  }
  task_icon_view_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kTasksIconRightPadding));

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

  list_footer_view_ = AddChildView(
      std::make_unique<GlanceablesListFooterView>(base::BindRepeating(
          &TasksBubbleView::ActionButtonPressed, base::Unretained(this))));

  ScheduleUpdateTasksList();
}

void TasksBubbleView::ActionButtonPressed() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kTasksManagementPage),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
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
  const int old_tasks_shown = num_tasks_shown_;
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

  list_footer_view_->UpdateItemsCount(num_tasks_shown_, num_tasks_);

  if (old_tasks_shown != num_tasks_shown_) {
    PreferredSizeChanged();
  }
}

BEGIN_METADATA(TasksBubbleView, views::View)
END_METADATA

}  // namespace ash
