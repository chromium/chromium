// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_task_view_v2.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "base/check.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kAddNewTaskIconSize = 24;
constexpr auto kHeaderIconButtonMargins = gfx::Insets::TLBR(0, 0, 0, 4);
constexpr int kInteriorGlanceableBubbleMargin = 16;
constexpr int kScrollViewBottomMargin = 12;
constexpr int kListViewBetweenChildSpacing = 4;
constexpr int kMaximumTasks = 100;
constexpr gfx::Insets kFooterBorderInsets = gfx::Insets::TLBR(4, 6, 8, 2);

constexpr char kTasksManagementPage[] =
    "https://calendar.google.com/calendar/u/0/r/week?opentasks=1";

class AddNewTaskButton : public views::LabelButton {
  METADATA_HEADER(AddNewTaskButton, views::LabelButton)
 public:
  explicit AddNewTaskButton(views::Button::PressedCallback callback)
      : views::LabelButton(
            std::move(callback),
            l10n_util::GetStringUTF16(
                IDS_GLANCEABLES_TASKS_ADD_NEW_TASK_BUTTON_LABEL)) {
    SetID(base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton));
    SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kGlanceablesTasksAddNewTaskIcon,
                      cros_tokens::kCrosSysPrimary, kAddNewTaskIconSize));
    SetImageLabelSpacing(14);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(8, 0)));
    SetEnabledTextColorIds(cros_tokens::kCrosSysPrimary);
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosButton2));
  }

  AddNewTaskButton(const AddNewTaskButton&) = delete;
  AddNewTaskButton& operator=(const AddNewTaskButton&) = delete;
  ~AddNewTaskButton() override = default;
};

BEGIN_METADATA(AddNewTaskButton)
END_METADATA

class TaskListScrollView : public views::ScrollView,
                           public views::ViewObserver {
  METADATA_HEADER(TaskListScrollView, views::ScrollView)
 public:
  TaskListScrollView() : scoped_observation_(this) {
    SetID(base::to_underlying(GlanceablesViewId::kTasksBubbleListScrollView));
    ClipHeightTo(0, std::numeric_limits<int>::max());
    SetBackgroundColor(std::nullopt);
    SetDrawOverflowIndicator(false);
  }

  TaskListScrollView(const TaskListScrollView&) = delete;
  TaskListScrollView& operator=(const TaskListScrollView&) = delete;
  ~TaskListScrollView() override = default;

  views::View* SetContents(std::unique_ptr<views::View> view) {
    views::View* contents = views::ScrollView::SetContents(std::move(view));
    scoped_observation_.Observe(contents);
    return contents;
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override {
    // Updates the preferred size of the scroll view when the content's
    // preferred size changed.
    if (contents_old_size_ != observed_view->size()) {
      contents_old_size_ = observed_view->size();
      PreferredSizeChanged();
    }
  }

 private:
  gfx::Size contents_old_size_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_;
};

BEGIN_METADATA(TaskListScrollView)
END_METADATA

}  // namespace

GlanceablesTasksViewBase::GlanceablesTasksViewBase()
    : GlanceableTrayChildBubble(/*for_glanceables_container=*/true) {}

BEGIN_METADATA(GlanceablesTasksViewBase)
END_METADATA

GlanceablesTasksView::GlanceablesTasksView(
    const ui::ListModel<api::TaskList>* task_lists) {
  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager
      ->SetInteriorMargin(gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin))
      .SetOrientation(views::LayoutOrientation::kVertical);

  // It is the parent container of GlanceablesTasksView that matches the style
  // of GlanceableTrayChildBubble. Manually update this bubble to match the
  // spec.
  CHECK(layer());
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{16.f});
  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemOnBase));
  SetBorder(nullptr);

  tasks_header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_header_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  tasks_header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_header_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderView));

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  auto* const scroll_view =
      AddChildView(std::make_unique<TaskListScrollView>());

  auto* const list_view =
      scroll_view->SetContents(std::make_unique<views::View>());
  scroll_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/
      gfx::Insets::TLBR(0, 0, kScrollViewBottomMargin, 0),
      kListViewBetweenChildSpacing));

  add_new_task_button_ =
      list_view->AddChildView(std::make_unique<AddNewTaskButton>(
          base::BindRepeating(&GlanceablesTasksView::AddNewTaskButtonPressed,
                              base::Unretained(this))));
  // Hide `add_new_task_button_` until the initial task list update.
  add_new_task_button_->SetVisible(false);

  task_items_container_view_ =
      list_view->AddChildView(std::make_unique<views::View>());
  task_items_container_view_->SetAccessibleRole(ax::mojom::Role::kList);
  task_items_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer));
  task_items_container_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          kListViewBetweenChildSpacing));

  auto* const header_icon =
      tasks_header_view_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                              base::Unretained(this),
                              TasksLaunchSource::kHeaderButton),
          IconButton::Type::kSmall, &kGlanceablesTasksIcon,
          IDS_GLANCEABLES_TASKS_HEADER_ICON_ACCESSIBLE_NAME));
  header_icon->SetBackgroundColor(SK_ColorTRANSPARENT);
  header_icon->SetProperty(views::kMarginsKey, kHeaderIconButtonMargins);
  header_icon->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderIcon));

  tasks_combobox_model_ = std::make_unique<TasksComboboxModel>(task_lists);
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

  list_footer_view_ =
      list_view->AddChildView(std::make_unique<GlanceablesListFooterView>(
          l10n_util::GetStringUTF16(
              IDS_GLANCEABLES_TASKS_SEE_ALL_BUTTON_ACCESSIBLE_NAME),
          base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                              base::Unretained(this),
                              TasksLaunchSource::kFooterButton)));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter));
  list_footer_view_->SetBorder(views::CreateEmptyBorder(kFooterBorderInsets));
  list_footer_view_->SetVisible(false);

  ScheduleUpdateTasksList(/*initial_update=*/true);
}

GlanceablesTasksView::~GlanceablesTasksView() {
  if (first_task_list_shown_) {
    RecordTasksListChangeCount(tasks_list_change_count_);
    RecordNumberOfAddedTasks(added_tasks_, task_list_initially_empty_,
                             user_with_no_tasks_);
  }
}

void GlanceablesTasksView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void GlanceablesTasksView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GlanceablesTasksView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, task_list_combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

void GlanceablesTasksView::AddNewTaskButtonPressed() {
  const auto* const active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  // TODO(b/301253574): make sure there is only one view is in `kEdit` state.
  auto* const pending_new_task = task_items_container_view_->AddChildViewAt(
      CreateTaskView(active_task_list->id, /*task=*/nullptr),
      /*index=*/0);
  pending_new_task->UpdateTaskTitleViewForState(
      GlanceablesTaskViewV2::TaskTitleViewState::kEdit);

  RecordUserStartedAddingTask();

  PreferredSizeChanged();
}

std::unique_ptr<GlanceablesTaskViewV2> GlanceablesTasksView::CreateTaskView(
    const std::string& task_list_id,
    const api::Task* task) {
  return std::make_unique<GlanceablesTaskViewV2>(
      task,
      base::BindRepeating(&GlanceablesTasksView::MarkTaskAsCompleted,
                          base::Unretained(this), task_list_id),
      base::BindRepeating(&GlanceablesTasksView::SaveTask,
                          base::Unretained(this), task_list_id),
      base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                          base::Unretained(this),
                          TasksLaunchSource::kEditInGoogleTasksButton));
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

  const auto* const active_task_list = tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
  tasks_combobox_model_->SaveLastSelectedTaskList(active_task_list->id);
  Shell::Get()->glanceables_controller()->GetTasksClient()->GetTasks(
      active_task_list->id,
      base::BindOnce(&GlanceablesTasksView::UpdateTasksList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id,
                     active_task_list->title, initial_update));
}

void GlanceablesTasksView::UpdateTasksList(
    const std::string& task_list_id,
    const std::string& task_list_title,
    bool initial_update,
    const ui::ListModel<api::Task>* tasks) {
  const gfx::Size old_preferred_size = GetPreferredSize();

  if (initial_update) {
    add_new_task_button_->SetVisible(true);
    base::UmaHistogramCounts100(
        "Ash.Glanceables.TimeManagement.TasksCountInDefaultTaskList",
        tasks->item_count());
  } else {
    RecordNumberOfAddedTasks(added_tasks_, task_list_initially_empty_,
                             user_with_no_tasks_);
    added_tasks_ = 0;
  }

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  task_items_container_view_->RemoveAllChildViews();

  size_t num_tasks_shown = 0;
  user_with_no_tasks_ =
      tasks->item_count() == 0 && tasks_combobox_model_->GetItemCount() == 1;

  for (const auto& task : *tasks) {
    if (task->completed) {
      continue;
    }

    if (num_tasks_shown < kMaximumTasks) {
      task_items_container_view_->AddChildView(
          CreateTaskView(task_list_id, task.get()));
      ++num_tasks_shown;
    }
  }
  task_list_initially_empty_ = num_tasks_shown == 0;
  list_footer_view_->SetVisible(tasks->item_count() >= kMaximumTasks);

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

void GlanceablesTasksView::ActionButtonPressed(TasksLaunchSource source) {
  if (user_with_no_tasks_) {
    RecordUserWithNoTasksRedictedToTasksUI();
  }
  RecordTasksLaunchSource(source);
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kTasksManagementPage),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void GlanceablesTasksView::SaveTask(
    const std::string& task_list_id,
    base::WeakPtr<GlanceablesTaskViewV2> view,
    const std::string& task_id,
    const std::string& title,
    api::TasksClient::OnTaskSavedCallback callback) {
  if (task_id.empty()) {
    // Manually deleting `view` may cause the focus manager try storing the
    // dangling `view`'s descendants. Let native window handle the view deletion
    // when it lost active.
    if (GetWidget() &&
        GetWidget()->GetNativeWindow() !=
            Shell::Get()->focus_controller()->GetActiveWindow()) {
      return;
    }

    // Empty `task_id` means that the task has not yet been created. Verify that
    // this task has a non-empty title, otherwise just delete the `view` from
    // the scrollable container.
    if (title.empty() && view) {
      RecordTaskAdditionResult(TaskModificationResult::kCancelled);
      task_items_container_view_->RemoveChildViewT(view.get());
      return;
    }

    ++added_tasks_;
    RecordTaskAdditionResult(TaskModificationResult::kCommitted);
  }

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);

  auto* const client = Shell::Get()->glanceables_controller()->GetTasksClient();
  auto on_task_saved = base::BindOnce(
      &GlanceablesTasksView::OnTaskSaved, weak_ptr_factory_.GetWeakPtr(),
      std::move(view), task_id, std::move(callback));
  if (task_id.empty()) {
    client->AddTask(task_list_id, title, std::move(on_task_saved));
  } else {
    client->UpdateTask(task_list_id, task_id, title, std::move(on_task_saved));
  }
}

void GlanceablesTasksView::OnTaskSaved(
    base::WeakPtr<GlanceablesTaskViewV2> view,
    const std::string& task_id,
    api::TasksClient::OnTaskSavedCallback callback,
    const api::Task* task) {
  if (!task) {
    ShowErrorMessage(u"[l10n] Error");
    if (task_id.empty() && view) {
      // Empty `task_id` means that the task has not yet been created. Delete
      // the corresponding `view` from the scrollable container in case of
      // error.
      task_items_container_view_->RemoveChildViewT(view.get());
    }
  } else if (task->title.empty()) {
    task_items_container_view_->RemoveChildViewT(view.get());
  }
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);
  std::move(callback).Run(task);
  list_footer_view_->SetVisible(task_items_container_view_->children().size() >=
                                kMaximumTasks);
}

BEGIN_METADATA(GlanceablesTasksView, views::View)
END_METADATA

}  // namespace ash
