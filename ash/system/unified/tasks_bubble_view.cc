// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/tasks_bubble_view.h"

#include <memory>
#include <string>

#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

constexpr int kMaximumTasks = 5;
constexpr int kTasksIconRightPadding = 14;
constexpr int kTasksIconViewSize = 32;
constexpr int kInteriorGlanceableBubbleMargin = 16;
constexpr auto kAddNewTaskButtonMargins = gfx::Insets::TLBR(0, 0, 16, 0);

constexpr char kTasksManagementPage[] =
    "https://calendar.google.com/calendar/u/0/r/week?opentasks=1";

}  // namespace

namespace ash {

TasksBubbleView::TasksBubbleView(DetailedViewDelegate* delegate)
    : GlanceableTrayChildBubble(delegate) {
  if (Shell::Get()->glanceables_v2_controller()->GetTasksClient()) {
    Shell::Get()->glanceables_v2_controller()->GetTasksClient()->GetTaskLists(
        base::BindOnce(&TasksBubbleView::InitViews,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

TasksBubbleView::~TasksBubbleView() = default;

void TasksBubbleView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, task_list_combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
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

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  task_items_container_view_ = AddChildView(std::make_unique<views::View>());
  task_items_container_view_->SetAccessibleRole(ax::mojom::Role::kList);

  task_items_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer));
  task_items_container_view_->SetPaintToLayer();
  task_items_container_view_->layer()->SetFillsBoundsOpaquely(false);
  task_items_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  auto* layout = task_items_container_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(2);

  add_new_task_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&TasksBubbleView::ActionButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ADD_NEW_TASK_BUTTON_LABEL)));
  add_new_task_button_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton));
  add_new_task_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kGlanceablesTasksAddNewTaskIcon,
                                     cros_tokens::kCrosSysOnSurface));
  add_new_task_button_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  add_new_task_button_->SetImageLabelSpacing(8);
  add_new_task_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, 16));
  add_new_task_button_->SetTextColorId(views::Button::ButtonState::STATE_NORMAL,
                                       cros_tokens::kCrosSysOnSurface);
  add_new_task_button_->SetProperty(views::kMarginsKey,
                                    kAddNewTaskButtonMargins);

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
  task_list_combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox));
  task_list_combo_box_view_->SetSizeToLargestLabel(false);
  combobox_view_observation_.Observe(task_list_combo_box_view_);

  // TODO(b/294681832): Finalize, and then localize strings.
  task_list_combo_box_view_->SetTooltipTextAndAccessibleName(
      u"Google tasks list");
  task_list_combo_box_view_->SetAccessibleDescription(u"");
  task_list_combo_box_view_->SetCallback(base::BindRepeating(
      &TasksBubbleView::SelectedTasksListChanged, base::Unretained(this)));
  task_list_combo_box_view_->SetSelectedIndex(0);

  list_footer_view_ = AddChildView(
      std::make_unique<GlanceablesListFooterView>(base::BindRepeating(
          &TasksBubbleView::ActionButtonPressed, base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter));

  SelectedTasksListChanged();
}

void TasksBubbleView::ActionButtonPressed() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kTasksManagementPage),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void TasksBubbleView::SelectedTasksListChanged() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  ScheduleUpdateTasksList();
}

void TasksBubbleView::ScheduleUpdateTasksList() {
  if (!task_list_combo_box_view_->GetSelectedIndex().has_value()) {
    return;
  }

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);
  task_list_combo_box_view_->SetAccessibleDescription(u"");

  GlanceablesTaskList* active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  Shell::Get()->glanceables_v2_controller()->GetTasksClient()->GetTasks(
      active_task_list->id,
      base::BindOnce(&TasksBubbleView::UpdateTasksList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id,
                     active_task_list->title));
}

void TasksBubbleView::UpdateTasksList(const std::string& task_list_id,
                                      const std::string& task_list_title,
                                      ui::ListModel<GlanceablesTask>* tasks) {
  const gfx::Size old_preferred_size = GetPreferredSize();
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  task_items_container_view_->RemoveAllChildViews();

  num_tasks_shown_ = 0;
  num_tasks_ = 0;
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
  task_items_container_view_->SetVisible(num_tasks_shown_ > 0);
  add_new_task_button_->SetVisible(num_tasks_shown_ == 0);

  list_footer_view_->UpdateItemsCount(num_tasks_shown_, num_tasks_);
  list_footer_view_->SetVisible(num_tasks_shown_ > 0);

  // TODO(b/294681832): Finalize, and then localize strings.
  task_items_container_view_->SetAccessibleName(base::UTF8ToUTF16(
      base::StringPrintf("Tasks list: %s", task_list_title.c_str())));
  task_items_container_view_->SetAccessibleDescription(
      list_footer_view_->items_count_label());
  task_items_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();
  }
}

void TasksBubbleView::AnnounceListStateOnComboBoxAccessibility() {
  if (add_new_task_button_->GetVisible()) {
    // TODO(b/294681832): Finalize, and then localize strings.
    task_list_combo_box_view_->GetViewAccessibility().AnnounceText(
        u"Selected list empty, navigate down to add a new task");
  } else if (list_footer_view_->items_count_label()->GetVisible()) {
    task_list_combo_box_view_->GetViewAccessibility().AnnounceText(
        list_footer_view_->items_count_label()->GetText());
  }
}

BEGIN_METADATA(TasksBubbleView, views::View)
END_METADATA

}  // namespace ash
