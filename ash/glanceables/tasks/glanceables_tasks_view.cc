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
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_combobox_model.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
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
#include "url/gurl.h"

namespace ash {
namespace {

constexpr auto kProgressBarPreferredSize = gfx::Size(0, 8);
constexpr auto kHeaderIconButtonMargins = gfx::Insets::TLBR(0, 0, 0, 2);
// The interior margin should be 12, but keep one pixel in the child views to
// set as the border so that it can accommodate the focus ring.
constexpr int kInteriorGlanceableBubbleMargin = 11;
constexpr int kScrollViewBottomMargin = 12;
constexpr int kListViewBetweenChildSpacing = 4;
constexpr int kMaximumTasks = 100;
constexpr gfx::Insets kFooterBorderInsets = gfx::Insets::TLBR(4, 6, 8, 2);

constexpr char kTasksManagementPage[] = "https://tasks.google.com/";

api::TasksClient* GetTasksClient() {
  return Shell::Get()->glanceables_controller()->GetTasksClient();
}

// Returns a displayable last modified time for kCantUpdateList.
std::u16string GetLastUpdateTimeMessage(base::Time time) {
  const std::u16string time_of_day = base::TimeFormatTimeOfDay(time);
  const std::u16string relative_date =
      ui::TimeFormat::RelativeDate(time, nullptr);
  if (relative_date.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_ERROR_LAST_UPDATE_DATE_AND_TIME, time_of_day,
        base::TimeFormatShortDate(time));
  }

  const auto midnight_today = base::Time::Now().LocalMidnight();
  const auto midnight_tomorrow = midnight_today + base::Days(1);
  if (midnight_today <= time && time < midnight_tomorrow) {
    return l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_ERROR_LAST_UPDATE_TIME, time_of_day);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_ERROR_LAST_UPDATE_DATE_AND_TIME, time_of_day,
        relative_date);
  }
}

class AddNewTaskButton : public views::LabelButton {
  METADATA_HEADER(AddNewTaskButton, views::LabelButton)
 public:
  explicit AddNewTaskButton(views::Button::PressedCallback callback)
      : views::LabelButton(
            std::move(callback),
            l10n_util::GetStringUTF16(
                IDS_GLANCEABLES_TASKS_ADD_NEW_TASK_BUTTON_LABEL)) {
    SetID(base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton));
    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kGlanceablesTasksAddNewTaskIcon,
                                       cros_tokens::kCrosSysPrimary));
    SetImageLabelSpacing(12);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(8, 0)));
    SetEnabledTextColorIds(cros_tokens::kCrosSysPrimary);
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosButton2));
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
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

// It is the parent container of GlanceablesTasksView that matches the style
// of GlanceableTrayChildBubble, so `use_glanceables_container_style` is set to
// false here.
GlanceablesTasksView::GlanceablesTasksView(
    const ui::ListModel<api::TaskList>* task_lists)
    : GlanceableTrayChildBubble(/*use_glanceables_container_style=*/false),
      shown_time_(base::Time::Now()) {
  SetAccessibleRole(ax::mojom::Role::kGroup);

  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager
      ->SetInteriorMargin(gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin))
      .SetOrientation(views::LayoutOrientation::kVertical);

  tasks_header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_header_view_->SetInteriorMargin(gfx::Insets::TLBR(1, 1, 0, 1));
  tasks_header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_header_view_->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  tasks_header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_header_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderView));

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->SetPreferredSize(kProgressBarPreferredSize);
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
      gfx::Insets::TLBR(1, 1, kScrollViewBottomMargin, 1),
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
                              TasksLaunchSource::kHeaderButton,
                              GURL(kTasksManagementPage)),
          IconButton::Type::kSmall, &kGlanceablesTasksIcon,
          IDS_GLANCEABLES_TASKS_HEADER_ICON_ACCESSIBLE_NAME));
  header_icon->SetBackgroundColor(SK_ColorTRANSPARENT);
  header_icon->SetProperty(views::kMarginsKey, kHeaderIconButtonMargins);
  header_icon->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderIcon));

  tasks_combobox_model_ =
      std::make_unique<GlanceablesTasksComboboxModel>(task_lists);
  CreateComboBoxView();

  list_footer_view_ =
      list_view->AddChildView(std::make_unique<GlanceablesListFooterView>(
          l10n_util::GetStringUTF16(
              IDS_GLANCEABLES_TASKS_SEE_ALL_BUTTON_ACCESSIBLE_NAME),
          base::BindRepeating(&GlanceablesTasksView::ActionButtonPressed,
                              base::Unretained(this),
                              TasksLaunchSource::kFooterButton,
                              GURL(kTasksManagementPage))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter));
  list_footer_view_->SetBorder(views::CreateEmptyBorder(kFooterBorderInsets));
  list_footer_view_->SetVisible(false);

  const auto* active_task_list = GetActiveTaskList();
  auto* tasks =
      GetTasksClient()->GetCachedTasksInTaskList(active_task_list->id);
  if (tasks) {
    UpdateTasksInTaskList(active_task_list->id, active_task_list->title,
                          ListShownContext::kCachedList, /*fetch_success=*/true,
                          tasks);
  } else {
    ScheduleUpdateTasks(ListShownContext::kInitialList);
  }
}

GlanceablesTasksView::~GlanceablesTasksView() {
  RecordTotalShowTimeForTasks(base::Time::Now() - shown_time_);
  if (first_task_list_shown_) {
    RecordTasksListChangeCount(tasks_list_change_count_);
    RecordNumberOfAddedTasks(added_tasks_, task_list_initially_empty_,
                             user_with_no_tasks_);
  }
}

void GlanceablesTasksView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void GlanceablesTasksView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, task_list_combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

void GlanceablesTasksView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GlanceablesTasksView::UpdateTaskLists(
    const ui::ListModel<api::TaskList>* task_lists) {
  tasks_combobox_model_->UpdateTaskLists(task_lists);
  SetIsLoading(true);

  CHECK(tasks_combobox_model_->GetDefaultIndex().has_value());
  auto* active_task_list = tasks_combobox_model_->GetTaskListAt(
      tasks_combobox_model_->GetDefaultIndex().value());

  recreate_combobox_callback_ =
      base::BindOnce(&GlanceablesTasksView::CreateComboBoxView,
                     weak_ptr_factory_.GetWeakPtr());

  // Force fetch the updated tasks with the new active task list.
  GetTasksClient()->GetTasks(
      active_task_list->id, /*force_fetch=*/true,
      base::BindOnce(&GlanceablesTasksView::UpdateTasksInTaskList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id,
                     active_task_list->title, ListShownContext::kInitialList));
}

void GlanceablesTasksView::AddNewTaskButtonPressed() {
  // TODO(b/301253574): make sure there is only one view is in `kEdit` state.
  task_items_container_view_->SetVisible(true);
  auto* const pending_new_task = task_items_container_view_->AddChildViewAt(
      CreateTaskView(GetActiveTaskList()->id, /*task=*/nullptr),
      /*index=*/0);
  pending_new_task->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);

  RecordUserStartedAddingTask();

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
                          base::Unretained(this), task_list_id),
      base::BindRepeating(
          &GlanceablesTasksView::ActionButtonPressed, base::Unretained(this),
          TasksLaunchSource::kEditInGoogleTasksButton,
          task && task->web_view_link.is_valid() ? task->web_view_link
                                                 : GURL(kTasksManagementPage)),
      base::BindRepeating(&GlanceablesTasksView::ShowErrorMessageWithType,
                          base::Unretained(this)));
}

void GlanceablesTasksView::SelectedTasksListChanged() {
  if (!glanceables_util::IsNetworkConnected()) {
    // If the network is disconnected, cancel the list change and show the error
    // message.
    ShowErrorMessageWithType(
        GlanceablesTasksErrorType::kCantLoadTasksNoNetwork,
        GlanceablesErrorMessageView::ButtonActionType::kDismiss);
    task_list_combo_box_view_->SetSelectedIndex(cached_selected_list_index_);
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  tasks_requested_time_ = base::TimeTicks::Now();
  tasks_list_change_count_++;
  ScheduleUpdateTasks(ListShownContext::kUserSelectedList);
}

void GlanceablesTasksView::ScheduleUpdateTasks(ListShownContext context) {
  if (!task_list_combo_box_view_->GetSelectedIndex().has_value()) {
    return;
  }

  SetIsLoading(true);
  task_list_combo_box_view_->GetViewAccessibility().SetDescription(u"");

  const auto* const active_task_list = GetActiveTaskList();
  tasks_combobox_model_->SaveLastSelectedTaskList(active_task_list->id);
  GetTasksClient()->GetTasks(
      active_task_list->id, /*force_fetch=*/true,
      base::BindOnce(&GlanceablesTasksView::UpdateTasksInTaskList,
                     weak_ptr_factory_.GetWeakPtr(), active_task_list->id,
                     active_task_list->title, context));
}

void GlanceablesTasksView::RetryUpdateTasks(ListShownContext context) {
  MaybeDismissErrorMessage();
  ScheduleUpdateTasks(context);
}

void GlanceablesTasksView::UpdateTasksInTaskList(
    const std::string& task_list_id,
    const std::string& task_list_title,
    ListShownContext context,
    bool fetch_success,
    const ui::ListModel<api::Task>* tasks) {
  const gfx::Size old_preferred_size = GetPreferredSize();
  SetIsLoading(false);

  if (!recreate_combobox_callback_.is_null()) {
    std::move(recreate_combobox_callback_).Run();
  }

  if (!fetch_success) {
    switch (context) {
      case ListShownContext::kCachedList:
        // Cached list should always considered as successfully fetched.
        NOTREACHED_NORETURN();
      case ListShownContext::kInitialList: {
        if (GetTasksClient()->GetCachedTasksInTaskList(task_list_id)) {
          // Notify users the last updated time of the tasks with the cached
          // data.
          ShowErrorMessageWithType(
              GlanceablesTasksErrorType::kCantUpdateTasks,
              GlanceablesErrorMessageView::ButtonActionType::kReload);
        } else {
          // Notify users that the target list of tasks wasn't loaded and guide
          // them to retry.
          ShowErrorMessageWithType(
              GlanceablesTasksErrorType::kCantLoadTasks,
              GlanceablesErrorMessageView::ButtonActionType::kReload);
        }
        return;
      }
      case ListShownContext::kUserSelectedList:
        ShowErrorMessageWithType(
            GlanceablesTasksErrorType::kCantLoadTasks,
            GlanceablesErrorMessageView::ButtonActionType::kDismiss);
        task_list_combo_box_view_->SetSelectedIndex(
            cached_selected_list_index_);
        return;
    }
  }

  // Discard the fetched tasks that is not shown now.
  if (task_list_id != GetActiveTaskList()->id) {
    return;
  }

  switch (context) {
    case ListShownContext::kCachedList:
      break;
    case ListShownContext::kInitialList:
      base::UmaHistogramCounts100(
          "Ash.Glanceables.TimeManagement.TasksCountInDefaultTaskList",
          tasks->item_count());
      break;
    case ListShownContext::kUserSelectedList:
      RecordNumberOfAddedTasks(added_tasks_, task_list_initially_empty_,
                               user_with_no_tasks_);
      added_tasks_ = 0;
      break;
  }

  add_new_task_button_->SetVisible(true);
  task_items_container_view_->RemoveAllChildViews();
  cached_selected_list_index_ = task_list_combo_box_view_->GetSelectedIndex();

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
  // Set `task_items_container_view_` to invisible if there is no task so that
  // the layout manager won't include it as a visible view.
  task_items_container_view_->SetVisible(
      !task_items_container_view_->children().empty());
  list_footer_view_->SetVisible(tasks->item_count() >= kMaximumTasks);

  task_list_combo_box_view_->SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_GLANCEABLES_TASKS_DROPDOWN_ACCESSIBLE_NAME,
                                 base::UTF8ToUTF16(task_list_title)));
  task_items_container_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_TASKS_SELECTED_LIST_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(task_list_title)));
  task_items_container_view_->GetViewAccessibility().SetDescription(
      *list_footer_view_->items_count_label());
  task_items_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();
    if (context == ListShownContext::kUserSelectedList) {
      GetWidget()->LayoutRootViewIfNecessary();
      ScrollViewToVisible();
    }
  }

  switch (context) {
    case ListShownContext::kCachedList:
      break;
    case ListShownContext::kInitialList: {
      auto* controller = Shell::Get()->glanceables_controller();
      RecordTasksInitialLoadTime(
          /*first_occurrence=*/controller->bubble_shown_count() == 1,
          base::TimeTicks::Now() - controller->last_bubble_show_time());
      first_task_list_shown_ = true;
      break;
    }
    case ListShownContext::kUserSelectedList:
      RecordActiveTaskListChanged();
      RecordTasksChangeLoadTime(base::TimeTicks::Now() - tasks_requested_time_);
      first_task_list_shown_ = true;
      break;
  }
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
  GetTasksClient()->MarkAsCompleted(task_list_id, task_id, completed);
}

void GlanceablesTasksView::ActionButtonPressed(TasksLaunchSource source,
                                               const GURL& target_url) {
  if (user_with_no_tasks_) {
    RecordUserWithNoTasksRedictedToTasksUI();
  }
  RecordTasksLaunchSource(source);
  NewWindowDelegate::GetPrimary()->OpenUrl(
      target_url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void GlanceablesTasksView::SaveTask(
    const std::string& task_list_id,
    base::WeakPtr<GlanceablesTaskView> view,
    const std::string& task_id,
    const std::string& title,
    api::TasksClient::OnTaskSavedCallback callback) {
  if (task_id.empty()) {
    // Empty `task_id` means that the task has not yet been created. Verify that
    // this task has a non-empty title, otherwise just delete the `view` from
    // the scrollable container.
    if (title.empty() && view) {
      RecordTaskAdditionResult(TaskModificationResult::kCancelled);

      // Removing the task immediately may cause a crash when the task is saved
      // in response to the task title textfield losing focus, as it may result
      // in deleting focused view while the focus manager is handling focus
      // change to another view. b/324409607
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&GlanceablesTasksView::RemoveTaskView,
                                    weak_ptr_factory_.GetWeakPtr(), view));
      return;
    }

    ++added_tasks_;
    RecordTaskAdditionResult(TaskModificationResult::kCommitted);
  }

  SetIsLoading(true);

  auto* const client = GetTasksClient();
  auto on_task_saved = base::BindOnce(
      &GlanceablesTasksView::OnTaskSaved, weak_ptr_factory_.GetWeakPtr(),
      std::move(view), task_id, std::move(callback));
  if (task_id.empty()) {
    client->AddTask(task_list_id, title, std::move(on_task_saved));
  } else {
    client->UpdateTask(task_list_id, task_id, title, /*completed=*/false,
                       std::move(on_task_saved));
  }
}

void GlanceablesTasksView::OnTaskSaved(
    base::WeakPtr<GlanceablesTaskView> view,
    const std::string& task_id,
    api::TasksClient::OnTaskSavedCallback callback,
    const api::Task* task) {
  if (!task) {
    ShowErrorMessageWithType(
        GlanceablesTasksErrorType::kCantUpdateTitle,
        GlanceablesErrorMessageView::ButtonActionType::kDismiss);
    if (task_id.empty() && view) {
      // Empty `task_id` means that the task has not yet been created. Delete
      // the corresponding `view` from the scrollable container in case of
      // error.
      task_items_container_view_->RemoveChildViewT(view.get());
    }
  } else if (task->title.empty()) {
    task_items_container_view_->RemoveChildViewT(view.get());
  }
  SetIsLoading(false);
  std::move(callback).Run(task);
  task_items_container_view_->SetVisible(
      !task_items_container_view_->children().empty());
  list_footer_view_->SetVisible(task_items_container_view_->children().size() >=
                                kMaximumTasks);
}

const api::TaskList* GlanceablesTasksView::GetActiveTaskList() const {
  return tasks_combobox_model_->GetTaskListAt(
      task_list_combo_box_view_->GetSelectedIndex().value());
}

void GlanceablesTasksView::ShowErrorMessageWithType(
    GlanceablesTasksErrorType error_type,
    GlanceablesErrorMessageView::ButtonActionType button_type) {
  views::Button::PressedCallback callback;
  switch (button_type) {
    case GlanceablesErrorMessageView::ButtonActionType::kDismiss:
      callback =
          base::BindRepeating(&GlanceablesTasksView::MaybeDismissErrorMessage,
                              base::Unretained(this));
      break;
    case GlanceablesErrorMessageView::ButtonActionType::kReload:
      // This only used to reload the current shown list.
      callback = base::BindRepeating(&GlanceablesTasksView::RetryUpdateTasks,
                                     base::Unretained(this),
                                     ListShownContext::kInitialList);
      break;
  }
  ShowErrorMessage(GetErrorString(error_type), std::move(callback),
                   button_type);
}

std::u16string GlanceablesTasksView::GetErrorString(
    GlanceablesTasksErrorType error_type) const {
  switch (error_type) {
    case GlanceablesTasksErrorType::kCantUpdateTasks: {
      auto last_modified_time =
          GetTasksClient()->GetTasksLastUpdateTime(GetActiveTaskList()->id);
      CHECK(last_modified_time.has_value());
      return GetLastUpdateTimeMessage(last_modified_time.value());
    }
    case GlanceablesTasksErrorType::kCantLoadTasks:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_LOAD_ITEMS_FAILED);
    case GlanceablesTasksErrorType::kCantLoadTasksNoNetwork:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_LOAD_ITEMS_FAILED_WHILE_OFFLINE);
    case GlanceablesTasksErrorType::kCantMarkComplete:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_MARK_COMPLETE_FAILED);
    case GlanceablesTasksErrorType::kCantMarkCompleteNoNetwork:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_MARK_COMPLETE_FAILED_WHILE_OFFLINE);
    case GlanceablesTasksErrorType::kCantUpdateTitle:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_EDIT_TASK_FAILED);
    case GlanceablesTasksErrorType::kCantUpdateTitleNoNetwork:
      return l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_TASKS_ERROR_EDIT_TASK_FAILED_WHILE_OFFLINE);
  }
}

void GlanceablesTasksView::RemoveTaskView(
    base::WeakPtr<GlanceablesTaskView> task_view) {
  if (!task_view) {
    return;
  }

  if (task_view->Contains(GetFocusManager()->GetFocusedView())) {
    add_new_task_button_->RequestFocus();
  }
  task_items_container_view_->RemoveChildViewT(task_view.get());
  task_items_container_view_->SetVisible(
      !task_items_container_view_->children().empty());
  PreferredSizeChanged();
}

void GlanceablesTasksView::CreateComboBoxView() {
  if (task_list_combo_box_view_) {
    combobox_view_observation_.Reset();
    tasks_header_view_->RemoveChildViewT(
        std::exchange(task_list_combo_box_view_, nullptr));
  }

  task_list_combo_box_view_ = tasks_header_view_->AddChildView(
      std::make_unique<Combobox>(tasks_combobox_model_.get()));
  task_list_combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox));
  task_list_combo_box_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  combobox_view_observation_.Observe(task_list_combo_box_view_);

  // Assign a default value for tooltip and accessible text.
  task_list_combo_box_view_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_TASKS_DROPDOWN_ACCESSIBLE_NAME, u""));
  task_list_combo_box_view_->GetViewAccessibility().SetDescription(u"");
  task_list_combo_box_view_->SetSelectionChangedCallback(base::BindRepeating(
      &GlanceablesTasksView::SelectedTasksListChanged, base::Unretained(this)));
}

void GlanceablesTasksView::SetIsLoading(bool is_loading) {
  progress_bar_->UpdateProgressBarVisibility(is_loading);

  // Disable all events in the subtree if the data fetch is ongoing.
  SetCanProcessEventsWithinSubtree(!is_loading);
}

BEGIN_METADATA(GlanceablesTasksView)
END_METADATA

}  // namespace ash
