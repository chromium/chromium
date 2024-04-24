// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/api/tasks/tasks_types.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/date_helper.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

constexpr char kFormatterPattern[] = "EEE, MMM d";  // "Wed, Feb 28"

// Margins between icons and labels in `tasks_details_view_`.
constexpr auto kDetailItemsMargin = gfx::Insets::TLBR(0, 0, 0, 4);

constexpr auto kSingleRowInteriorMargin = gfx::Insets::TLBR(6, 0, 2, 0);
constexpr auto kDoubleRowInteriorMargin = gfx::Insets();

constexpr auto kCheckButtonMargin = gfx::Insets::VH(2, 0);
constexpr auto kContentsMargin = gfx::Insets::TLBR(0, 4, 4, 8);
// In edit state, the bottom margin is smaller to accommodate
// `edit_in_browser_button_`.
constexpr auto kContentsMarginInEditState = gfx::Insets::TLBR(0, 4, 0, 8);

constexpr auto kTitleAndDetailMarginsInViewState =
    gfx::Insets::TLBR(0, 8, 0, 0);
constexpr auto kTitleMarginsInEditState = gfx::Insets();
constexpr auto kDetailMarginsInEditState = gfx::Insets::TLBR(4, 8, 0, 0);
constexpr auto kEditInBrowserMargins = gfx::Insets::TLBR(8, 2, 0, 0);

views::Label* SetupLabel(views::View* parent) {
  views::Label* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Views should not be individually selected for accessibility. Accessible
  // name and behavior comes from the parent.
  label->GetViewAccessibility().SetIsIgnored(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetAutoColorReadabilityEnabled(false);
  return label;
}

std::u16string GetFormattedDueDate(const base::Time& due) {
  // Google Tasks API does not respect time portion of the date and always
  // returns "YYYY-MM-DDT00:00:00.000Z" format (see "due" field
  // https://developers.google.com/tasks/reference/rest/v1/tasks). Treating this
  // date in UTC format as is leads to showing one day less in timezones to the
  // west of UTC. The following line adjusts `due` so that it becomes a
  // **local** midnight instead.
  const auto adjusted_due = due - calendar_utils::GetTimeDifference(due);
  const auto midnight_today = base::Time::Now().LocalMidnight();
  const auto midnight_tomorrow = midnight_today + base::Days(1);

  if (midnight_today <= adjusted_due && adjusted_due < midnight_tomorrow) {
    return l10n_util::GetStringUTF16(IDS_GLANCEABLES_DUE_TODAY);
  }

  auto* const date_helper = DateHelper::GetInstance();
  CHECK(date_helper);
  const auto formatter =
      date_helper->CreateSimpleDateFormatter(kFormatterPattern);
  return date_helper->GetFormattedTime(&formatter, adjusted_due);
}

std::unique_ptr<views::ImageView> CreateSecondRowIcon(
    const gfx::VectorIcon& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      icon, cros_tokens::kCrosSysOnSurfaceVariant));
  return icon_view;
}

class TaskViewTextField : public SystemTextfield,
                          public SystemTextfieldController {
  METADATA_HEADER(TaskViewTextField, SystemTextfield)

 public:
  using OnFinishedEditingCallback =
      base::RepeatingCallback<void(const std::u16string& title)>;

  TaskViewTextField(const std::u16string& title,
                    OnFinishedEditingCallback on_finished_editing)
      : SystemTextfield(Type::kMedium),
        SystemTextfieldController(/*textfield=*/this),
        on_finished_editing_(std::move(on_finished_editing)) {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_GLANCEABLES_TASKS_TEXTFIELD_PLACEHOLDER));
    SetBackgroundColor(SK_ColorTRANSPARENT);
    SetController(this);
    SetID(base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField));
    SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_GLANCEABLES_TASKS_TEXTFIELD_PLACEHOLDER));
    SetText(title);
    SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosButton2));
    SetActiveStateChangedCallback(base::BindRepeating(
        &TaskViewTextField::OnActiveStateChanged, base::Unretained(this)));
  }
  TaskViewTextField(const TaskViewTextField&) = delete;
  TaskViewTextField& operator=(const TaskViewTextField&) = delete;
  ~TaskViewTextField() override = default;

  // SystemTextfield:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, TypographyProvider::Get()->ResolveLineHeight(
                            TypographyToken::kCrosButton2));
  }

 private:
  // SystemTextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    CHECK_EQ(this, sender);
    if (key_event.type() != ui::ET_KEY_PRESSED) {
      return false;
    }

    // Pressing escape should saves the change in the textfield. This works the
    // same as the textfield in Google Tasks.
    if (IsActive() && key_event.key_code() == ui::VKEY_ESCAPE) {
      // Commit the changes and deactivate the textfield.
      SetActive(false);
      return true;
    }

    return SystemTextfieldController::HandleKeyEvent(sender, key_event);
  }

  void OnActiveStateChanged() {
    // Entering inactive state from the active state implies the editing is
    // done.
    if (!IsActive()) {
      on_finished_editing_.Run(GetText());
    }
  }

  OnFinishedEditingCallback on_finished_editing_;
};

BEGIN_METADATA(TaskViewTextField)
END_METADATA

class EditInBrowserButton : public views::LabelButton {
  METADATA_HEADER(EditInBrowserButton, views::LabelButton)
 public:
  explicit EditInBrowserButton(PressedCallback callback)
      : views::LabelButton(std::move(callback),
                           l10n_util::GetStringUTF16(
                               IDS_GLANCEABLES_TASKS_EDIT_IN_TASKS_LABEL)) {
    SetID(base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(3, 6)));
    SetProperty(views::kMarginsKey, kEditInBrowserMargins);
    SetEnabledTextColorIds(cros_tokens::kCrosSysPrimary);
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosButton2));
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
    button_controller()->set_notify_action(
        views::ButtonController::NotifyAction::kOnPress);
  }
};

BEGIN_METADATA(EditInBrowserButton)
END_METADATA

}  // namespace

class GlanceablesTaskView::CheckButton : public views::ImageButton {
  METADATA_HEADER(CheckButton, views::ImageButton)

 public:
  explicit CheckButton(PressedCallback pressed_callback)
      : views::ImageButton(std::move(pressed_callback)) {
    SetBorder(views::CreateEmptyBorder(gfx::Insets(2)));
    SetAccessibleRole(ax::mojom::Role::kCheckBox);
    UpdateImage();
    SetFlipCanvasOnPaintForRTLUI(/*enable=*/false);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::ImageButton::GetAccessibleNodeData(node_data);

    node_data->SetName(l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_MARK_COMPLETED_ACCESSIBLE_NAME));

    const ax::mojom::CheckedState checked_state =
        checked_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse;
    node_data->SetCheckedState(checked_state);
    node_data->SetDefaultActionVerb(checked_
                                        ? ax::mojom::DefaultActionVerb::kUncheck
                                        : ax::mojom::DefaultActionVerb::kCheck);
  }

  void SetChecked(bool checked) {
    checked_ = checked;
    UpdateImage();
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  }

  bool checked() const { return checked_; }

 private:
  void UpdateImage() {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      checked_ ? kGlanceablesHollowCheckCircleIcon
                               : kGlanceablesHollowCircleIcon,
                      cros_tokens::kFocusRingColor));
  }

  bool checked_ = false;
};

BEGIN_METADATA(GlanceablesTaskView, CheckButton)
END_METADATA

class GlanceablesTaskView::TaskTitleButton : public views::LabelButton {
  METADATA_HEADER(TaskTitleButton, views::LabelButton)

 public:
  TaskTitleButton(const std::u16string& title, PressedCallback pressed_callback)
      : views::LabelButton(std::move(pressed_callback), title) {
    SetBorder(nullptr);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);

    label()->SetID(base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel));
    label()->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
        TypographyToken::kCrosButton2));
  }

  void UpdateLabelForState(bool completed) {
    const auto color_id = completed ? cros_tokens::kCrosSysSecondary
                                    : cros_tokens::kCrosSysOnSurface;
    SetEnabledTextColorIds(color_id);
    SetTextColorId(views::Button::ButtonState::STATE_DISABLED, color_id);
    label()->SetFontList(
        TypographyProvider::Get()
            ->ResolveTypographyToken(TypographyToken::kCrosButton2)
            .DeriveWithStyle(completed ? gfx::Font::FontStyle::STRIKE_THROUGH
                                       : gfx::Font::FontStyle::NORMAL));
  }
};

BEGIN_METADATA(GlanceablesTaskView, TaskTitleButton)
END_METADATA

GlanceablesTaskView::GlanceablesTaskView(
    const api::Task* task,
    MarkAsCompletedCallback mark_as_completed_callback,
    SaveCallback save_callback,
    base::RepeatingClosure edit_in_browser_callback,
    ShowErrorMessageCallback show_error_message_callback)
    : task_id_(task ? task->id : ""),
      task_title_(task ? base::UTF8ToUTF16(task->title) : u""),
      mark_as_completed_callback_(std::move(mark_as_completed_callback)),
      save_callback_(std::move(save_callback)),
      edit_in_browser_callback_(std::move(edit_in_browser_callback)),
      show_error_message_callback_(std::move(show_error_message_callback)) {
  SetAccessibleRole(ax::mojom::Role::kListItem);
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetOrientation(views::LayoutOrientation::kHorizontal);

  check_button_ =
      AddChildView(std::make_unique<CheckButton>(base::BindRepeating(
          &GlanceablesTaskView::CheckButtonPressed, base::Unretained(this))));
  check_button_->SetProperty(views::kMarginsKey, kCheckButtonMargin);

  contents_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  contents_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  contents_view_->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  contents_view_->SetOrientation(views::LayoutOrientation::kVertical);
  contents_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  tasks_title_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_title_view_->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  tasks_title_view_->SetProperty(views::kMarginsKey, gfx::Insets::VH(4, 0));

  tasks_details_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_details_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_details_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  tasks_details_view_->SetDefault(views::kMarginsKey, kDetailItemsMargin);

  std::vector<std::u16string> details;
  if (task && task->due.has_value()) {
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesTasksDueDateIcon));

    const auto formatted_due_date = GetFormattedDueDate(task->due.value());
    details.push_back(l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_DUE_DATE_ACCESSIBLE_DESCRIPTION,
        formatted_due_date));

    views::Label* due_date_label = SetupLabel(tasks_details_view_);
    due_date_label->SetText(formatted_due_date);
    due_date_label->SetID(
        base::to_underlying(GlanceablesViewId::kTaskItemDueLabel));
    due_date_label->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation1));
    due_date_label->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
        TypographyToken::kCrosAnnotation1));
    due_date_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  }

  if (task && task->has_subtasks) {
    details.push_back(l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_SUBTASK_ACCESSIBLE_DESCRIPTION));
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesSubtaskIcon));
  }

  if (task && task->has_notes) {
    details.push_back(l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_DETAILS_ACCESSIBLE_DESCRIPTION));
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesTasksNotesIcon));
  }

  UpdateTaskTitleViewForState(TaskTitleViewState::kView);

  // Use different margins depending on the number of
  // rows of text shown.
  const bool double_row = tasks_details_view_->children().size() > 0;
  SetInteriorMargin(double_row ? kDoubleRowInteriorMargin
                               : kSingleRowInteriorMargin);

  auto a11y_description = task_title_;
  if (!details.empty()) {
    a11y_description += u". ";
    a11y_description += l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_METADATA_WRAPPER_ACCESSIBLE_DESCRIPTION,
        base::JoinString(details, u", "));
  }
  check_button_->GetViewAccessibility().SetDescription(a11y_description);
  check_button_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

GlanceablesTaskView::~GlanceablesTaskView() = default;

void GlanceablesTaskView::OnViewBlurred(views::View* observed_view) {
  if ((observed_view == edit_in_browser_button_ ||
       observed_view == task_title_textfield_) &&
      (!edit_in_browser_button_ || !edit_in_browser_button_->HasFocus()) &&
      (!task_title_textfield_ || !task_title_textfield_->HasFocus())) {
    edit_exit_observer_.RemoveAllObservations();
    // Schedule a task to update task view state to kView. Done asynchronously
    // to let the focus change to complete. State update will remove the blurred
    // view while focus is still changing, which may cause a crash -
    // b/324409607.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlanceablesTaskView::UpdateTaskTitleViewForState,
                       state_change_weak_ptr_factory_.GetWeakPtr(),
                       TaskTitleViewState::kView));
  }
}

void GlanceablesTaskView::OnViewIsDeleting(views::View* observed_view) {
  edit_exit_observer_.RemoveObservation(observed_view);

  if (!edit_exit_observer_.IsObservingAnySource()) {
    UpdateTaskTitleViewForState(TaskTitleViewState::kView);
  }
}

const views::ImageButton* GlanceablesTaskView::GetCheckButtonForTest() const {
  return check_button_;
}

bool GlanceablesTaskView::GetCompletedForTest() const {
  return check_button_->checked();
}

void GlanceablesTaskView::UpdateTaskTitleViewForState(
    TaskTitleViewState state) {
  state_change_weak_ptr_factory_.InvalidateWeakPtrs();
  edit_exit_observer_.RemoveAllObservations();

  task_title_button_ = nullptr;
  task_title_textfield_ = nullptr;
  tasks_title_view_->RemoveAllChildViews();

  switch (state) {
    case TaskTitleViewState::kNotInitialized:
      NOTREACHED_NORETURN();
    case TaskTitleViewState::kView:
      if (contents_view_ && edit_in_browser_button_) {
        contents_view_->RemoveChildViewT(
            std::exchange(edit_in_browser_button_, nullptr));
      }
      task_title_button_ =
          tasks_title_view_->AddChildView(std::make_unique<TaskTitleButton>(
              task_title_,
              base::BindRepeating(&GlanceablesTaskView::TaskTitleButtonPressed,
                                  base::Unretained(this))));
      task_title_button_->SetEnabled(!saving_task_changes_);
      task_title_button_->UpdateLabelForState(
          /*completed=*/check_button_->checked());
      break;
    case TaskTitleViewState::kEdit:
      task_title_before_edit_ = task_title_;
      task_title_textfield_ =
          tasks_title_view_->AddChildView(std::make_unique<TaskViewTextField>(
              task_title_,
              base::BindRepeating(&GlanceablesTaskView::OnFinishedEditing,
                                  base::Unretained(this))));
      GetWidget()->widget_delegate()->SetCanActivate(true);
      task_title_textfield_->RequestFocus();

      edit_in_browser_button_ = contents_view_->AddChildView(
          std::make_unique<EditInBrowserButton>(edit_in_browser_callback_));
      edit_exit_observer_.AddObservation(task_title_textfield_);
      edit_exit_observer_.AddObservation(edit_in_browser_button_);
      break;
  }

  UpdateContentsMargins(state);
}

void GlanceablesTaskView::UpdateContentsMargins(TaskTitleViewState state) {
  switch (state) {
    case TaskTitleViewState::kNotInitialized:
      NOTREACHED_NORETURN();
    case TaskTitleViewState::kView:
      contents_view_->SetProperty(views::kMarginsKey, kContentsMargin);
      task_title_button_->SetProperty(views::kMarginsKey,
                                      kTitleAndDetailMarginsInViewState);
      tasks_details_view_->SetProperty(views::kMarginsKey,
                                       kTitleAndDetailMarginsInViewState);
      break;
    case TaskTitleViewState::kEdit:
      contents_view_->SetProperty(views::kMarginsKey,
                                  kContentsMarginInEditState);
      task_title_textfield_->SetProperty(views::kMarginsKey,
                                         kTitleMarginsInEditState);
      tasks_details_view_->SetProperty(views::kMarginsKey,
                                       kDetailMarginsInEditState);
      break;
  }
}

void GlanceablesTaskView::CheckButtonPressed() {
  if (!glanceables_util::IsNetworkConnected()) {
    show_error_message_callback_.Run(
        GlanceablesTasksErrorType::kCantMarkCompleteNoNetwork,
        GlanceablesErrorMessageView::ButtonActionType::kDismiss);
    return;
  }

  if (saving_task_changes_) {
    return;
  }

  bool target_state = !check_button_->checked();
  check_button_->SetChecked(target_state);

  if (task_title_button_) {
    task_title_button_->UpdateLabelForState(/*completed=*/target_state);
  }
  RecordTaskMarkedAsCompleted(target_state);
  mark_as_completed_callback_.Run(task_id_, /*completed=*/target_state);
}

void GlanceablesTaskView::TaskTitleButtonPressed() {
  if (!glanceables_util::IsNetworkConnected()) {
    show_error_message_callback_.Run(
        GlanceablesTasksErrorType::kCantUpdateTitleNoNetwork,
        GlanceablesErrorMessageView::ButtonActionType::kDismiss);
    return;
  }
  RecordUserModifyingTask();

  UpdateTaskTitleViewForState(TaskTitleViewState::kEdit);
}

void GlanceablesTaskView::OnFinishedEditing(const std::u16string& title) {
  if (!title.empty()) {
    task_title_ = title;
  }

  if (task_title_textfield_ && task_title_textfield_->HasFocus()) {
    GetFocusManager()->ClearFocus();
  }

  if (task_id_.empty() || task_title_ != task_title_before_edit_) {
    saving_task_changes_ = true;
    if (task_title_button_) {
      task_title_button_->SetEnabled(false);
    }
    if (task_title_textfield_) {
      task_title_textfield_->SetEnabled(false);
    }
    // Note: result for task addition flow will be recorded in the parent view,
    // which initialized add task flow.
    if (!task_id_.empty()) {
      RecordTaskModificationResult(TaskModificationResult::kCommitted);
    }
    save_callback_.Run(weak_ptr_factory_.GetWeakPtr(), task_id_,
                       base::UTF16ToUTF8(task_title_),
                       base::BindOnce(&GlanceablesTaskView::OnSaved,
                                      weak_ptr_factory_.GetWeakPtr()));
    // TODO(b/301253574): introduce "disabled" state for this view to prevent
    // editing / marking as complete while the task is not fully created yet and
    // race conditions while editing the same task.
  } else {
    // Note: result for task addition flow will be recorded in the parent view,
    // which initialized add task flow.
    if (!task_id_.empty()) {
      RecordTaskModificationResult(TaskModificationResult::kCancelled);
    }
  }
}

void GlanceablesTaskView::OnSaved(const api::Task* task) {
  saving_task_changes_ = false;
  if (task_title_button_) {
    task_title_button_->SetEnabled(true);
  }
  if (task_title_textfield_) {
    task_title_textfield_->SetEnabled(true);
  }
  if (task) {
    task_id_ = task->id;
  } else {
    // If `task` is a nullptr, the change failed to commit.
    task_title_ = task_title_before_edit_;
    task_title_button_->SetText(task_title_);
  }
  task_title_before_edit_ = u"";
}

BEGIN_METADATA(GlanceablesTaskView)
END_METADATA

}  // namespace ash
