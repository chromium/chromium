// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_view.h"

#include <memory>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/icon_button.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kMaximumTasks = 5;
constexpr int kInteriorGlanceableBubbleMargin = 16;
constexpr int kAddNewButtonCornerRadius = 16;
constexpr auto kAddNewTaskButtonMargins = gfx::Insets::TLBR(0, 0, 16, 0);
constexpr auto kHeaderIconButtonMargins = gfx::Insets::TLBR(0, 0, 0, 4);

constexpr char kTasksManagementPage[] =
    "https://calendar.google.com/calendar/u/0/r/week?opentasks=1";

std::unique_ptr<views::LabelButton> CreateAddNewTaskButton(
    views::Button::PressedCallback callback) {
  auto add_new_task_button = std::make_unique<views::LabelButton>(
      std::move(callback),
      l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ADD_NEW_TASK_BUTTON_LABEL));
  add_new_task_button->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton));
  add_new_task_button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kGlanceablesTasksAddNewTaskIcon,
                                     cros_tokens::kCrosSysOnSurface));
  add_new_task_button->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  add_new_task_button->SetImageLabelSpacing(8);
  add_new_task_button->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kAddNewButtonCornerRadius));
  add_new_task_button->SetTextColorId(views::Button::ButtonState::STATE_NORMAL,
                                      cros_tokens::kCrosSysOnSurface);
  add_new_task_button->SetProperty(views::kMarginsKey,
                                   kAddNewTaskButtonMargins);

  views::FocusRing::Get(add_new_task_button.get())
      ->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::HighlightPathGenerator::Install(
      add_new_task_button.get(),
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), kAddNewButtonCornerRadius));

  return add_new_task_button;
}

}  // namespace

GlanceablesTasksViewBase::GlanceablesTasksViewBase(
    DetailedViewDelegate* delegate)
    : GlanceableTrayChildBubble(delegate, /*for_glanceables_container=*/true) {}

GlanceablesTasksView::GlanceablesTasksView(
    DetailedViewDelegate* delegate,
    ui::ListModel<api::TaskList>* task_list)
    : GlanceablesTasksViewBase(delegate) {
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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  tasks_header_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderView));

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  add_new_task_button_ = AddChildView(CreateAddNewTaskButton(
      base::BindRepeating(&GlanceablesTasksView::AddNewTaskButtonPressed,
                          base::Unretained(this))));

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

  auto* const header_icon =
      tasks_header_view_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                              base::Unretained(this),
                              TasksLaunchSource::kHeaderButton),
          IconButton::Type::kMedium, &kGlanceablesTasksIcon,
          IDS_GLANCEABLES_TASKS_HEADER_ICON_ACCESSIBLE_NAME));
  header_icon->SetBackgroundColorId(cros_tokens::kCrosSysBaseElevated);
  header_icon->SetProperty(views::kMarginsKey, kHeaderIconButtonMargins);
  header_icon->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderIcon));

  tasks_combobox_model_ = std::make_unique<TasksComboboxModel>(task_list);
  task_list_combo_box_view_ = tasks_header_view_->AddChildView(
      std::make_unique<Combobox>(tasks_combobox_model_.get()));
  task_list_combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox));
  task_list_combo_box_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  combobox_view_observation_.Observe(task_list_combo_box_view_);

  task_list_combo_box_view_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_TASKS_DROPDOWN_ACCESSIBLE_NAME));
  task_list_combo_box_view_->SetAccessibleDescription(u"");
  task_list_combo_box_view_->SetSelectionChangedCallback(base::BindRepeating(
      &GlanceablesTasksView::SelectedTasksListChanged, base::Unretained(this)));

  list_footer_view_ = AddChildView(std::make_unique<GlanceablesListFooterView>(
      l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_SEE_ALL_BUTTON_ACCESSIBLE_NAME),
      base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                          base::Unretained(this),
                          TasksLaunchSource::kFooterButton)));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter));

  ScheduleUpdateTasksList(/*initial_update=*/true);
}

GlanceablesTasksView::~GlanceablesTasksView() {
  if (first_task_list_shown_) {
    RecordTasksListChangeCount(tasks_list_change_count_);
  }
}

void GlanceablesTasksView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GlanceablesTasksView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, task_list_combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

void GlanceablesTasksView::ActionButtonPressed(TasksLaunchSource source) {
  RecordTasksLaunchSource(source);
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kTasksManagementPage),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void GlanceablesTasksView::AddNewTaskButtonPressed() {
  const auto* const active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  // TODO(b/301253574): make sure there is only one view is in `kEdit` state.
  auto* const pending_task = task_items_container_view_->AddChildViewAt(
      CreateTaskView(active_task_list->id, /*task=*/nullptr),
      /*index=*/0);
  pending_task->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PreferredSizeChanged();
}

std::unique_ptr<GlanceablesTaskView> GlanceablesTasksView::CreateTaskView(
    const std::string& task_list_id,
    const api::Task* task) {
  return std::make_unique<GlanceablesTaskView>(
      task,
      base::BindRepeating(&GlanceablesTasksView::MarkTaskAsCompleted,
                          base::Unretained(this), task_list_id),
      base::BindRepeating(&GlanceablesTasksView::SaveTask,
                          base::Unretained(this), task_list_id));
}

void GlanceablesTasksView::SelectedTasksListChanged() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  tasks_requested_time_ = base::TimeTicks::Now();
  tasks_list_change_count_++;
  ScheduleUpdateTasksList(/*initial_update=*/false);
}

void GlanceablesTasksView::ScheduleUpdateTasksList(bool initial_update) {
  if (!task_list_combo_box_view_->GetSelectedIndex().has_value()) {
    return;
  }

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);
  task_list_combo_box_view_->SetAccessibleDescription(u"");

  api::TaskList* active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  tasks_combobox_model_->SaveLastSelectedTaskList(active_task_list->id);
  Shell::Get()->glanceables_controller()->GetTasksClient()->GetTasks(
      active_task_list->id,
      base::BindOnce(&GlanceablesTasksView::UpdateTasksList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id,
                     active_task_list->title, initial_update));
}

void GlanceablesTasksView::UpdateTasksList(const std::string& task_list_id,
                                           const std::string& task_list_title,
                                           bool initial_update,
                                           ui::ListModel<api::Task>* tasks) {
  if (initial_update) {
    base::UmaHistogramCounts100(
        "Ash.Glanceables.TimeManagement.TasksCountInDefaultTaskList",
        tasks->item_count());
  }

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
      task_items_container_view_->AddChildView(
          CreateTaskView(task_list_id, task.get()));
      ++num_tasks_shown_;
    }
    ++num_tasks_;
  }
  task_items_container_view_->SetVisible(num_tasks_shown_ > 0);

  list_footer_view_->UpdateItemsCount(num_tasks_shown_, num_tasks_);
  list_footer_view_->SetVisible(num_tasks_shown_ > 0);

  task_items_container_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_TASKS_SELECTED_LIST_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(task_list_title)));
  task_items_container_view_->SetAccessibleDescription(
      list_footer_view_->items_count_label());
  task_items_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();
    if (!initial_update) {
      GetWidget()->LayoutRootViewIfNecessary();
      ScrollViewToVisible();
    }
  }

  auto* controller = Shell::Get()->glanceables_controller();

  if (initial_update) {
    RecordTasksInitialLoadTime(
        /* first_occurrence=*/controller->bubble_shown_count() == 1,
        base::TimeTicks::Now() - controller->last_bubble_show_time());
  } else {
    RecordActiveTaskListChanged();
    RecordTasksChangeLoadTime(base::TimeTicks::Now() - tasks_requested_time_);
  }

  first_task_list_shown_ = true;
}

void GlanceablesTasksView::AnnounceListStateOnComboBoxAccessibility() {
  if (list_footer_view_->items_count_label()->GetVisible()) {
    task_list_combo_box_view_->GetViewAccessibility().AnnounceText(
        list_footer_view_->items_count_label()->GetText());
  }
}

void GlanceablesTasksView::MarkTaskAsCompleted(const std::string& task_list_id,
                                               const std::string& task_id,
                                               bool completed) {
  Shell::Get()->glanceables_controller()->GetTasksClient()->MarkAsCompleted(
      task_list_id, task_id, completed);
}

void GlanceablesTasksView::SaveTask(const std::string& task_list_id,
                                    const std::string& task_id,
                                    const std::string& title) {
  // TODO(b/301253574): show/hide `progress_bar_` and/or an error message.
  auto* const client = Shell::Get()->glanceables_controller()->GetTasksClient();
  if (task_id.empty()) {
    client->AddTask(task_list_id, title);
  } else {
    client->UpdateTask(task_list_id, task_id, title, base::DoNothing());
  }
}

BEGIN_METADATA(GlanceablesTasksView, views::View)
END_METADATA

}  // namespace ash
