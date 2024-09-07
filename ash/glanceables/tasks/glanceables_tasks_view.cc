// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/glanceables/common/glanceables_contents_scroll_view.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
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
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
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
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kListViewBetweenChildSpacing = 4;
constexpr int kMaximumTasks = 100;

constexpr base::TimeDelta kBubbleExpandAnimationDuration =
    base::Milliseconds(300);
constexpr base::TimeDelta kBubbleCollapseAnimationDuration =
    base::Milliseconds(250);
constexpr gfx::Tween::Type kBubbleAnimationTweenType =
    gfx::Tween::FAST_OUT_SLOW_IN;

constexpr char kTasksManagementPage[] = "https://tasks.google.com/";

constexpr char kExpandAnimationSmoothnessHistogramName[] =
    "Ash.Glanceables.TimeManagement.Tasks.Expand.AnimationSmoothness";
constexpr char kCollapseAnimationSmoothnessHistogramName[] =
    "Ash.Glanceables.TimeManagement.Tasks.Collapse.AnimationSmoothness";
constexpr char kChildResizingAnimationSmoothnessHistogramName[] =
    "Ash.Glanceables.TimeManagement.Tasks.ChildResizing.AnimationSmoothness";

// Returns the InitParams for constructor.
GlanceablesTasksView::InitParams CreateInitParamsForTasks(
    const ui::ListModel<api::TaskList>* task_lists) {
  GlanceablesTasksView::InitParams init_params;
  init_params.context = GlanceablesTasksView::Context::kTasks;
  init_params.combobox_model =
      std::make_unique<GlanceablesTasksComboboxModel>(task_lists);
  init_params.combobox_tooltip = l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_TASKS_DROPDOWN_ACCESSIBLE_NAME, u"");
  init_params.expand_button_tooltip_id =
      IDS_GLANCEABLES_TASKS_EXPAND_BUTTON_EXPAND_TOOLTIP;
  init_params.collapse_button_tooltip_id =
      IDS_GLANCEABLES_TASKS_EXPAND_BUTTON_COLLAPSE_TOOLTIP;
  init_params.footer_title = l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_LIST_FOOTER_SEE_ALL_TASKS_LABEL);
  init_params.footer_tooltip = l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_TASKS_SEE_ALL_BUTTON_ACCESSIBLE_NAME);
  init_params.header_icon = &kGlanceablesTasksIcon;
  init_params.header_icon_tooltip_id =
      IDS_GLANCEABLES_TASKS_HEADER_ICON_ACCESSIBLE_NAME;
  return init_params;
}

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
    SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(4, 0, 8, 0));
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

}  // namespace

GlanceablesTasksView::GlanceablesTasksView(
    const ui::ListModel<api::TaskList>* task_lists)
    : GlanceablesTimeManagementBubbleView(CreateInitParamsForTasks(task_lists)),
      shown_time_(base::Time::Now()) {
  // Caches the combobox model pointer for later model updates.
  tasks_combobox_model_ =
      static_cast<GlanceablesTasksComboboxModel*>(combobox_model());

  add_new_task_button_ = content_scroll_view()->contents()->AddChildViewAt(
      std::make_unique<AddNewTaskButton>(
          base::BindRepeating(&GlanceablesTasksView::AddNewTaskButtonPressed,
                              base::Unretained(this))),
      0);
  // Hide `add_new_task_button_` until the initial task list update.
  add_new_task_button_->SetVisible(false);

  const auto* active_task_list = GetActiveTaskList();
  auto* tasks =
      GetTasksClient()->GetCachedTasksInTaskList(active_task_list->id);
  if (tasks) {
    UpdateTasksInTaskList(active_task_list->id, active_task_list->title,
                          ListShownContext::kCachedList, /*fetch_success=*/true,
                          std::nullopt, tasks);
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

void GlanceablesTasksView::Layout(PassKey) {
  LayoutSuperclass<GlanceablesTimeManagementBubbleView>(this);

  // Set the sentinel bounds to track the bottom of the last task view. Note
  // that the sentinel is the last view in `task_view_model_`.
  if (task_list_sentinel_) {
    if (task_view_model_.view_size() > 1) {
      auto* last_task =
          task_view_model_.view_at(task_view_model_.view_size() - 2);
      task_list_sentinel_->SetBoundsRect(gfx::Rect(
          gfx::Point(
              0, last_task->bounds().bottom() + kListViewBetweenChildSpacing),
          task_list_sentinel_->GetPreferredSize()));
    } else {
      task_list_sentinel_->SetBoundsRect(
          gfx::Rect(task_list_sentinel_->GetPreferredSize()));
    }
  }
}

gfx::Size GlanceablesTasksView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (running_resize_animation_.has_value() &&
      *running_resize_animation_ == ResizeAnimation::Type::kChildResize) {
    // The animation was implemented to ignore `available_size`. See b/351880846
    // for more detail.
    const gfx::Size base_preferred_size =
        views::FlexLayoutView::CalculatePreferredSize({});

    // If bottom of the task list is animating, offset the tasks view
    // preferred size so the tasks matches the animating bottom of the task
    // list. This reduces animation jankiness of the timing of the resize
    // animation gets slightly out of sync with the task views animations.
    const int sentinel_offset =
        task_list_sentinel_ && task_list_sentinel_->layer()
            ? task_list_sentinel_->layer()->transform().To2dTranslation().y()
            : 0;
    if (sentinel_offset != 0) {
      return gfx::Size(base_preferred_size.width(),
                       base_preferred_size.height() + sentinel_offset);
    }
  }

  return GlanceablesTimeManagementBubbleView::CalculatePreferredSize(
      available_size);
}

void GlanceablesTasksView::AnimationEnded(const gfx::Animation* animation) {
  running_resize_animation_.reset();
  GlanceablesTimeManagementBubbleView::AnimationEnded(animation);
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

void GlanceablesTasksView::EndResizeAnimationForTest() {
  if (resize_animation_) {
    resize_animation_->End();
  }
}

void GlanceablesTasksView::OnHeaderIconPressed() {
  ActionButtonPressed(TasksLaunchSource::kHeaderButton,
                      GURL(kTasksManagementPage));
}

void GlanceablesTasksView::OnFooterButtonPressed() {
  ActionButtonPressed(TasksLaunchSource::kFooterButton,
                      GURL(kTasksManagementPage));
}

void GlanceablesTasksView::SelectedListChanged() {
  if (!glanceables_util::IsNetworkConnected()) {
    // If the network is disconnected, cancel the list change and show the error
    // message.
    ShowErrorMessageWithType(GlanceablesTasksErrorType::kCantLoadTasksNoNetwork,
                             ErrorMessageToast::ButtonActionType::kDismiss);
    combobox_view()->SetSelectedIndex(cached_selected_list_index_);
    return;
  }

  UpdateComboboxReplacementLabelText();

  weak_ptr_factory_.InvalidateWeakPtrs();
  tasks_requested_time_ = base::TimeTicks::Now();
  tasks_list_change_count_++;
  ScheduleUpdateTasks(ListShownContext::kUserSelectedList);
}

void GlanceablesTasksView::AnimateResize(ResizeAnimation::Type resize_type) {
  const int current_height = size().height();
  if (current_height == 0) {
    return;
  }

  // Child resize animation should not override the expand/collapse animation.
  if (resize_type == ResizeAnimation::Type::kChildResize &&
      running_resize_animation_.has_value() &&
      *running_resize_animation_ ==
          ResizeAnimation::Type::kContainerExpandStateChanged) {
    return;
  }

  resize_animation_.reset();
  running_resize_animation_.reset();

  if (!ui::ScopedAnimationDurationScaleMode::duration_multiplier()) {
    PreferredSizeChanged();
    return;
  }

  // Check if the available height is large enough for the preferred height, so
  // that the target height for the animation is correctly bounded.
  const views::SizeBound available_height =
      parent()->GetAvailableSize(this).height();
  const int preferred_height = GetPreferredSize().height();
  const int target_height =
      available_height.is_bounded()
          ? std::min(available_height.value(), preferred_height)
          : preferred_height;
  if (current_height == target_height) {
    return;
  }

  // If the scroll view is in overflow, and is expected to remain in overflow
  // after resizing, no need to animate the bubble size, as it's not actually
  // going to change.
  const int visible_scroll_height =
      content_scroll_view()->GetVisibleRect().height();
  if (resize_type == ResizeAnimation::Type::kChildResize &&
      content_scroll_view()->contents()->height() > visible_scroll_height &&
      content_scroll_view()->contents()->GetPreferredSize().height() >
          visible_scroll_height) {
    PreferredSizeChanged();
    return;
  }

  switch (resize_type) {
    case ResizeAnimation::Type::kContainerExpandStateChanged:
      SetUpResizeThroughputTracker(
          target_height > current_height
              ? kExpandAnimationSmoothnessHistogramName
              : kCollapseAnimationSmoothnessHistogramName);
      break;
    case ResizeAnimation::Type::kChildResize:
      SetUpResizeThroughputTracker(
          kChildResizingAnimationSmoothnessHistogramName);
      break;
  }
  running_resize_animation_ = resize_type;
  resize_animation_ = std::make_unique<ResizeAnimation>(
      current_height, target_height, this, resize_type);
  resize_animation_->Start();
}

void GlanceablesTasksView::AddNewTaskButtonPressed() {
  // TODO(b/301253574): make sure there is only one view is in `kEdit` state.
  items_container_view()->SetVisible(true);
  auto* const pending_new_task = items_container_view()->AddChildViewAt(
      CreateTaskView(GetActiveTaskList()->id, /*task=*/nullptr),
      /*index=*/0);
  task_view_model_.Add(pending_new_task, 0u);
  pending_new_task->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  AnimateTaskViewVisibility(pending_new_task, true);
  HandleTaskViewStateChange(/*view_expanding=*/true);
  RecordUserStartedAddingTask();
}

std::unique_ptr<GlanceablesTaskView> GlanceablesTasksView::CreateTaskView(
    const std::string& task_list_id,
    const api::Task* task) {
  auto task_view = std::make_unique<GlanceablesTaskView>(
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
  if (task) {
    // Start observing for state changes after setting the default task view
    // state.
    task_view->set_state_change_observer(
        base::BindRepeating(&GlanceablesTasksView::HandleTaskViewStateChange,
                            base::Unretained(this)));
  }
  return task_view;
}

void GlanceablesTasksView::ScheduleUpdateTasks(ListShownContext context) {
  if (!combobox_view()->GetSelectedIndex().has_value()) {
    return;
  }

  SetIsLoading(true);
  combobox_view()->GetViewAccessibility().SetDescription(u"");

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
    std::optional<google_apis::ApiErrorCode> http_error,
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
        NOTREACHED();
      case ListShownContext::kInitialList: {
        if (GetTasksClient()->GetCachedTasksInTaskList(task_list_id)) {
          // Notify users the last updated time of the tasks with the cached
          // data.
          ShowErrorMessageWithType(
              GlanceablesTasksErrorType::kCantUpdateTasks,
              ErrorMessageToast::ButtonActionType::kReload);
        } else {
          // Notify users that the target list of tasks wasn't loaded and guide
          // them to retry.
          ShowErrorMessageWithType(
              GlanceablesTasksErrorType::kCantLoadTasks,
              ErrorMessageToast::ButtonActionType::kReload);
        }
        return;
      }
      case ListShownContext::kUserSelectedList:
        ShowErrorMessageWithType(GlanceablesTasksErrorType::kCantLoadTasks,
                                 ErrorMessageToast::ButtonActionType::kDismiss);
        combobox_view()->SetSelectedIndex(cached_selected_list_index_);
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

  task_list_sentinel_ = nullptr;
  task_view_model_.Clear();
  cached_selected_list_index_ = combobox_view()->GetSelectedIndex();

  size_t num_tasks_shown = 0;
  user_with_no_tasks_ =
      tasks->item_count() == 0 && tasks_combobox_model_->GetItemCount() == 1;

  items_container_view()->SetVisible(false);
  for (const auto& task : *tasks) {
    if (task->completed) {
      continue;
    }

    if (num_tasks_shown < kMaximumTasks) {
      auto* task_view = items_container_view()->AddChildView(
          CreateTaskView(task_list_id, task.get()));
      task_view_model_.Add(task_view, task_view_model_.view_size());
      ++num_tasks_shown;
    }
  }

  items_container_view()->SetVisible(true);

  task_list_sentinel_ = AddChildView(std::make_unique<views::View>());
  task_list_sentinel_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  // The view is hidden, and excluded from the layout - size is set to an
  // arbitrary non empty value.
  task_list_sentinel_->SetPreferredSize(gfx::Size(1, 1));
  task_list_sentinel_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  task_list_sentinel_->layer()->SetFillsBoundsOpaquely(false);
  task_list_sentinel_->layer()->SetOpacity(0.0f);
  task_view_model_.Add(task_list_sentinel_.get(), task_view_model_.view_size());

  task_list_initially_empty_ = num_tasks_shown == 0;
  // Set `task_items_container_view()` to invisible if there is no task so that
  // the layout manager won't include it as a visible view.
  items_container_view()->SetVisible(
      !items_container_view()->children().empty());
  list_footer_view()->SetVisible(tasks->item_count() >= kMaximumTasks);
  expand_button()->UpdateCounter(tasks->item_count());

  combobox_view()->SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_GLANCEABLES_TASKS_DROPDOWN_ACCESSIBLE_NAME,
                                 base::UTF8ToUTF16(task_list_title)));
  items_container_view()->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_GLANCEABLES_TASKS_SELECTED_LIST_ACCESSIBLE_NAME,
          base::UTF8ToUTF16(task_list_title)));
  items_container_view()->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  if (old_preferred_size != GetPreferredSize()) {
    if (context == ListShownContext::kUserSelectedList) {
      AnimateResize(ResizeAnimation::Type::kChildResize);
    } else {
      PreferredSizeChanged();
    }
  }

  // Scroll the scroll view back to the top after the selected list is changed.
  content_scroll_view()->ScrollToOffset(gfx::PointF(0, 0));

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

void GlanceablesTasksView::HandleTaskViewStateChange(bool view_expanding) {
  // NOTE: A single event may cause one task view to expand, and another one to
  // collapse - the animation will use parameters for the event that comes in
  // later, which depends on how the views handle events.
  auto animation = views::AnimationBuilder();
  animation.Once().SetDuration(view_expanding
                                   ? kBubbleExpandAnimationDuration
                                   : kBubbleCollapseAnimationDuration);

  int target_offset = 0;
  for (size_t i = 0; i < task_view_model_.view_size(); ++i) {
    auto* child = task_view_model_.view_at(i);

    // Calculate the current apparent bounds for the view - the view transform
    // is set to animate it to its ideal bounds, which may differ from the
    // current view bounds if a layout is still pending.
    gfx::Rect current_bounds = task_view_model_.ideal_bounds(i);
    if (current_bounds.height() == 0) {
      current_bounds = child->bounds();
    }
    int current_offset =
        current_bounds.y() + child->layer()->transform().To2dTranslation().y();

    // Update the child transform to be relative to it's new target bounds,
    // which are expected to be set after an imminent layout pass.
    child->layer()->SetTransform(
        gfx::Transform::MakeTranslation(0, current_offset - target_offset));

    // Animate the view into the expected new position.
    if (current_offset != target_offset) {
      animation.GetCurrentSequence().SetTransform(child, gfx::Transform(),
                                                  kBubbleAnimationTweenType);
    }

    const gfx::Size preferred_size = child->GetPreferredSize();
    // Cache the bounds relative to which the transform was calculated, so they
    // can be used to update the view transform if a view state changes before
    // the imminent layout pass.
    task_view_model_.set_ideal_bounds(
        i, gfx::Rect(gfx::Point(current_bounds.x(), target_offset),
                     preferred_size));

    // Update the target offset for the next task view in the task list.
    target_offset += preferred_size.height() + kListViewBetweenChildSpacing;
  }

  AnimateResize(ResizeAnimation::Type::kChildResize);
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

      // Prevent editing the task view when `view` is waiting to be deleted.
      view->SetCanProcessEventsWithinSubtree(false);

      // Removing the task immediately may cause a crash when the task is saved
      // in response to the task title textfield losing focus, as it may result
      // in deleting focused view while the focus manager is handling focus
      // change to another view. b/324409607
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&GlanceablesTasksView::RemoveTaskView,
                                    weak_ptr_factory_.GetWeakPtr(), view));
      return;
    }

    if (view) {
      view->set_state_change_observer(
          base::BindRepeating(&GlanceablesTasksView::HandleTaskViewStateChange,
                              base::Unretained(this)));
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
    google_apis::ApiErrorCode http_error,
    const api::Task* task) {
  if (!task) {
    ShowErrorMessageWithType(GlanceablesTasksErrorType::kCantUpdateTitle,
                             ErrorMessageToast::ButtonActionType::kDismiss);
    if (task_id.empty() && view) {
      // Empty `task_id` means that the task has not yet been created. Delete
      // the corresponding `view` from the scrollable container in case of
      // error.
      RemoveTaskView(view);
    }
  } else if (task->title.empty()) {
    RemoveTaskView(view);
  }
  SetIsLoading(false);
  std::move(callback).Run(http_error, task);

  const size_t tasks_count = items_container_view()->children().size();
  items_container_view()->SetVisible(tasks_count > 0);
  expand_button()->UpdateCounter(tasks_count);
  list_footer_view()->SetVisible(tasks_count >= kMaximumTasks);
}

const api::TaskList* GlanceablesTasksView::GetActiveTaskList() const {
  return tasks_combobox_model_->GetTaskListAt(GetComboboxSelectedIndex());
}

void GlanceablesTasksView::ShowErrorMessageWithType(
    GlanceablesTasksErrorType error_type,
    ErrorMessageToast::ButtonActionType button_type) {
  views::Button::PressedCallback callback;
  switch (button_type) {
    case ErrorMessageToast::ButtonActionType::kDismiss:
      callback =
          base::BindRepeating(&GlanceablesTasksView::MaybeDismissErrorMessage,
                              base::Unretained(this));
      break;
    case ErrorMessageToast::ButtonActionType::kReload:
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
      if (!last_modified_time.has_value()) {
        return l10n_util::GetStringUTF16(
            IDS_GLANCEABLES_TASKS_ERROR_LOAD_ITEMS_FAILED);
      }
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

  AnimateTaskViewVisibility(task_view.get(), false);

  auto index_in_model = task_view_model_.GetIndexOfView(task_view.get());
  if (index_in_model) {
    task_view_model_.Remove(*index_in_model);
  }
  items_container_view()->RemoveChildViewT(task_view.get());
  HandleTaskViewStateChange(/*view_expanding=*/false);

  items_container_view()->SetVisible(
      !items_container_view()->children().empty());
}

void GlanceablesTasksView::SetIsLoading(bool is_loading) {
  progress_bar()->UpdateProgressBarVisibility(is_loading);

  // Disable all events in the subtree if the data fetch is ongoing.
  SetCanProcessEventsWithinSubtree(!is_loading);
}

void GlanceablesTasksView::AnimateTaskViewVisibility(views::View* task,
                                                     bool visible) {
  if (!visible) {
    animating_task_view_layer_ = task->RecreateLayer();
  } else {
    task->layer()->SetOpacity(animating_task_view_layer_
                                  ? animating_task_view_layer_->opacity()
                                  : 0.0f);
    animating_task_view_layer_.reset();
  }

  task->SetVisible(visible);

  // Animate the transform back to the identity transform.
  views::AnimationBuilder()
      .OnEnded(
          base::BindOnce(&GlanceablesTasksView::OnTaskViewAnimationCompleted,
                         weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&GlanceablesTasksView::OnTaskViewAnimationCompleted,
                         weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .At(visible ? base::Milliseconds(50) : base::TimeDelta())
      .SetOpacity(visible ? task->layer() : animating_task_view_layer_.get(),
                  visible ? 1.0f : 0.0f, gfx::Tween::LINEAR)
      .SetDuration(base::Milliseconds(visible ? 100 : 50));
}

void GlanceablesTasksView::OnTaskViewAnimationCompleted() {
  animating_task_view_layer_.reset();
}

BEGIN_METADATA(GlanceablesTasksView)
END_METADATA

}  // namespace ash
