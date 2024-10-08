// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_task_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_chip_carousel.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kIconSize = 20;
constexpr int kTextfieldCornerRadius = 8;
constexpr float kOfflineStateOpacity = 0.38f;
constexpr auto kSelectedStateBoxInsets = gfx::Insets::VH(4, 0);
constexpr auto kSelectedStateTextfieldInsets = gfx::Insets::TLBR(0, 16, 0, 12);
constexpr auto kUnselectedStateBoxInsets = gfx::Insets::TLBR(4, 8, 4, 16);
constexpr auto kUnselectedStateTextfieldInsets = gfx::Insets::TLBR(0, 8, 0, 0);

constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);

// Clears the focus away from `textfield`.
void ClearFocusForTextfield(views::Textfield* textfield) {
  auto* focus_manager = textfield->GetWidget()->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same view when the parent view is
  // refocused.
  focus_manager->SetStoredFocusView(nullptr);
}

// Returns true if ChromeVox (spoken feedback) is enabled.
bool IsSpokenFeedbackEnabled() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

}  // namespace

//---------------------------------------------------------------------
// FocusModeTaskView::TaskTextfield:

class FocusModeTaskView::TaskTextfield : public SystemTextfield {
  METADATA_HEADER(TaskTextfield, SystemTextfield)

 public:
  // The `kMedium` type of `SystemTextfield` has a 20px font size and a 28px
  // container height.
  explicit TaskTextfield(base::RepeatingClosure callback)
      : SystemTextfield(SystemTextfield::Type::kMedium) {
    // `SystemTextfield` separates the "focused" and "active" states. The
    // textfield can be focused but inactive. For example, while editing, if the
    // user presses Enter, it will commit the changes and deactivate the
    // textfield, but the textfield is also focused such that we can re-activate
    // it by pressing Enter agagin. In focused mode case, we only want to show
    // the focus ring when the textfield is active. Thus, we will paint the
    // focus ring of `parent()` each time on the active state changed.
    SetActiveStateChangedCallback(std::move(callback));

    UpdateElideBehavior(IsActive());
  }
  TaskTextfield(const TaskTextfield&) = delete;
  TaskTextfield& operator=(const TaskTextfield&) = delete;
  ~TaskTextfield() override = default;

  // The max number of characters (UTF-16) allowed for the textfield.
  static constexpr size_t kMaxLength = 1023;

  void set_show_selected_state(bool show_selected_state) {
    if (show_selected_state_ && !show_selected_state) {
      // If transitioning from selected to unselected, remove the focus ring.
      SetShowFocusRing(false);
    }
    show_selected_state_ = show_selected_state;
  }

  bool show_selected() const { return show_selected_state_; }

  std::u16string GetTooltipText() const { return tooltip_text_; }

  void SetTooltipText(const std::u16string& tooltip_text) {
    if (tooltip_text_ == tooltip_text) {
      return;
    }

    tooltip_text_ = tooltip_text;
    TooltipTextChanged();
    OnPropertyChanged(&tooltip_text_, views::kPropertyEffectsNone);
  }

  void UpdateElideBehavior(bool active) {
    GetRenderText()->SetElideBehavior(active ? gfx::NO_ELIDE : gfx::ELIDE_TAIL);
  }

  // views::View:
  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return tooltip_text_;
  }

 private:
  // True if `FocusModeTaskView` has a selected task.
  bool show_selected_state_ = false;

  std::u16string tooltip_text_;
};

BEGIN_METADATA(FocusModeTaskView, TaskTextfield)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
END_METADATA

//---------------------------------------------------------------------
// FocusModeTaskView::TaskTextfieldController:

class FocusModeTaskView::TaskTextfieldController
    : public SystemTextfieldController,
      public views::ViewObserver {
 public:
  TaskTextfieldController(SystemTextfield* textfield, FocusModeTaskView* owner)
      : SystemTextfieldController(textfield),
        textfield_(textfield),
        owner_(owner) {
    textfield_->AddObserver(this);
  }
  TaskTextfieldController(const TaskTextfieldController&) = delete;
  TaskTextfieldController& operator=(const TaskTextfieldController&) = delete;
  ~TaskTextfieldController() override { textfield_->RemoveObserver(this); }

  // SystemTextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    DCHECK_EQ(sender, textfield_);

    // Google Tasks have a max length for each task title, so we trim if needed
    // at kMaxLength UTF-16 boundary.
    if (new_contents.size() > TaskTextfield::kMaxLength) {
      textfield_->SetText(new_contents.substr(0, TaskTextfield::kMaxLength));
    }
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::EventType::kKeyPressed &&
        key_event.key_code() == ui::VKEY_RETURN) {
      // If the textfield is focused but not active, activate the textfield and
      // highlight all the text.
      if (!textfield_->IsActive()) {
        textfield_->SetActive(true);
        textfield_->SelectAll(/*reversed=*/false);
        return true;
      }

      ClearFocusForTextfield(textfield_);
      return true;
    }

    // TODO(b/306271947): Verify the `ESC` key to restore the text when the user
    // edits a task.
    return SystemTextfieldController::HandleKeyEvent(sender, key_event);
  }

  // views::ViewObserver:
  void OnViewBlurred(views::View* view) override {
    owner_->CommitTextfieldContents(textfield_->GetText());
  }

 private:
  const raw_ptr<SystemTextfield> textfield_;

  // The owning `FocusModeTaskView`.
  const raw_ptr<FocusModeTaskView> owner_;
};

//---------------------------------------------------------------------
// FocusModeTaskView:

FocusModeTaskView::FocusModeTaskView(bool is_network_connected)
    : is_network_connected_(is_network_connected) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  textfield_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  textfield_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  textfield_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  complete_button_ = textfield_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &FocusModeTaskView::OnCompleteTask, base::Unretained(this))));
  const std::u16string radio_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_VIEW_RADIO_BUTTON);
  complete_button_->GetViewAccessibility().SetName(radio_text);
  complete_button_->SetTooltipText(radio_text);

  views::FocusRing::Install(complete_button_);
  views::FocusRing::Get(complete_button_)
      ->SetColorId(cros_tokens::kCrosSysFocusRing);

  add_task_button_ = textfield_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &FocusModeTaskView::OnAddTaskButtonPressed, base::Unretained(this))));
  add_task_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kGlanceablesTasksAddNewTaskIcon,
                                     is_network_connected
                                         ? cros_tokens::kCrosSysSecondary
                                         : cros_tokens::kCrosSysDisabled,
                                     kIconSize));
  add_task_button_->SetFlipCanvasOnPaintForRTLUI(false);
  add_task_button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  // Ignore `add_task_button_`for accessibility purposes.
  add_task_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kNone);
  add_task_button_->SetEnabled(is_network_connected);

  textfield_ =
      textfield_container_->AddChildView(std::make_unique<TaskTextfield>(
          base::BindRepeating(&FocusModeTaskView::PaintFocusRingAndUpdateStyle,
                              weak_factory_.GetWeakPtr())));
  textfield_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetBackgroundEnabled(false);
  textfield_->UpdateBackground();
  textfield_->SetPlaceholderText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetPlaceholderTextColorId(is_network_connected
                                            ? cros_tokens::kCrosSysSecondary
                                            : cros_tokens::kCrosSysDisabled);
  if (!is_network_connected) {
    textfield_->SetEnabled(false);
    textfield_->SetPaintToLayer();
    // Make the layer transparent.
    textfield_->layer()->SetFillsBoundsOpaquely(false);
    textfield_->layer()->SetOpacity(kOfflineStateOpacity);
  }
  // Shrink the inactive `textfield_` ring so it's not touching the other views
  // when focused.
  views::InstallRoundRectHighlightPathGenerator(
      textfield_, gfx::Insets::VH(0, 8), kTextfieldCornerRadius);

  textfield_container_->SetFlexForView(textfield_, 1);
  // We only show `textfield_container_`'s focus ring when the textfield is
  // active.
  views::FocusRing::Install(textfield_container_);
  // Set the focus ring corner radius with 8px.
  views::InstallRoundRectHighlightPathGenerator(
      textfield_container_, gfx::Insets(), kTextfieldCornerRadius);
  auto* textfield_container_focus_ring =
      views::FocusRing::Get(textfield_container_);
  textfield_container_focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  textfield_container_focus_ring->SetOutsetFocusRingDisabled(true);
  // `textfield_container_` has the focus ring only when `textfield_` is active
  // and isn't in selected state.
  textfield_container_focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](const TaskTextfield* textfield, const views::View* view) {
        return textfield && textfield->IsActive() &&
               !textfield->show_selected();
      },
      textfield_));

  deselect_button_ =
      textfield_container_->AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&FocusModeTaskView::OnDeselectButtonPressed,
                              base::Unretained(this))));
  deselect_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kMediumOrLargeCloseButtonIcon,
                                     is_network_connected
                                         ? cros_tokens::kCrosSysSecondary
                                         : cros_tokens::kCrosSysDisabled,
                                     kIconSize));
  deselect_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_DESELECT_BUTTON));
  deselect_button_->SetEnabled(is_network_connected);
  views::FocusRing::Install(deselect_button_);
  views::FocusRing::Get(deselect_button_)
      ->SetColorId(cros_tokens::kCrosSysFocusRing);

  chip_carousel_ = AddChildView(std::make_unique<FocusModeChipCarousel>(
      base::BindRepeating(&FocusModeTaskView::OnTaskSelectedFromCarousel,
                          base::Unretained(this))));

  // Initialize styling as unselected.
  UpdateStyle(/*show_selected_state=*/false, is_network_connected);

  textfield_controller_ =
      std::make_unique<TaskTextfieldController>(textfield_, this);

  auto* controller = FocusModeController::Get();
  tasks_observation_.Observe(&controller->tasks_model());

  controller->tasks_model().RequestUpdate();
}

FocusModeTaskView::~FocusModeTaskView() = default;

void FocusModeTaskView::OnSelectedTaskChanged(
    const std::optional<FocusModeTask>& task) {
  if (!task) {
    task_id_.reset();

    // Apply the UI updates if the completion animation is not running.
    // Otherwise, it'll be updated by `OnClearTask()`.
    if (!complete_animation_running_) {
      textfield_->SetText(std::u16string());
      if (textfield_->HasFocus()) {
        textfield_->SetActive(true);
      }
      UpdateStyle(/*show_selected_state=*/false, is_network_connected_);
    }
    return;
  }

  const bool show_selected_state = !task->title.empty();
  if (show_selected_state) {
    task_id_ = std::make_optional(task->task_id);
    textfield_->SetText(base::UTF8ToUTF16(task->title));
  }

  UpdateStyle(/*show_selected_state=*/show_selected_state,
              /*is_network_connected=*/is_network_connected_);
}

void FocusModeTaskView::OnTasksUpdated(
    const std::vector<FocusModeTask>& tasks) {
  chip_carousel_->SetTasks(tasks);
  chip_carousel_->SetVisible(!textfield_->show_selected() && !tasks.empty());
}

void FocusModeTaskView::OnTaskCompleted(const FocusModeTask& task) {
  // If there was no task selected, update to the default state.
  if (!task_id_.has_value()) {
    OnSelectedTaskChanged(std::nullopt);
    return;
  }

  // Save that the complete animation is running so we can skip the selected
  // task change event.
  complete_animation_running_ = true;

  // Implement completed task styling before removing the task with an
  // animation.
  complete_button_->SetEnabled(false);
  complete_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kDoneIcon, cros_tokens::kCrosSysPrimary,
                                     kIconSize));
  textfield_->SetFontList(
      TypographyProvider::Get()
          ->ResolveTypographyToken(TypographyToken::kCrosBody2)
          .DeriveWithStyle(gfx::Font::FontStyle::STRIKE_THROUGH));
  textfield_->SetTextColorId(cros_tokens::kCrosSysSecondary);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FocusModeTaskView::OnClearTask,
                     weak_factory_.GetWeakPtr()),
      kStartAnimationDelay);
}

void FocusModeTaskView::OnTaskSelectedFromCarousel(
    const FocusModeTask& task_entry) {
  if (task_entry.task_id.empty() || task_entry.title.empty()) {
    OnClearTask();
    return;
  }

  FocusModeController::Get()->tasks_model().SetSelectedTask(task_entry.task_id);

  // When ChromeVox is on, after selecting a task from the chip carousel we
  // should set focus on the `complete_button_`.
  if (IsSpokenFeedbackEnabled()) {
    complete_button_->RequestFocus();
  }
}

void FocusModeTaskView::OnClearTask() {
  // Clear the complete animation.
  complete_animation_running_ = false;
  if (!task_id_.has_value()) {
    // If a task is not already selected, there is no event for the change in
    // selected task (because it was already cleared). Trigger the UI update
    // manually.
    OnSelectedTaskChanged(std::nullopt);
    return;
  }
  FocusModeController::Get()->tasks_model().ClearSelectedTask();
}

SystemTextfield* FocusModeTaskView::GetTaskTextfieldForTesting() {
  return textfield_;
}

void FocusModeTaskView::CommitTextfieldContents(
    const std::u16string& contents) {
  // Textfield blur is triggered before we know if a chip has been clicked. If a
  // chip was clicked, we ignore what was in the textfield. Post the update so
  // it runs after the click would be processed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FocusModeTaskView::AddOrUpdateTask,
                     weak_factory_.GetWeakPtr(), task_id_, contents));
}

void FocusModeTaskView::AddOrUpdateTask(const std::optional<TaskId>& task_id,
                                        const std::u16string& task_title) {
  if (task_id_ != task_id) {
    // Since the event was queued, the selected task has changed. Discard this
    // update in favor of the other event.
    return;
  }

  if (task_title.empty()) {
    OnClearTask();
    return;
  }

  const bool prev_complete_button_visibility = complete_button_->GetVisible();

  FocusModeTasksModel::TaskUpdate update;
  if (task_id_ && !task_id_->empty()) {
    update.task_id = std::make_optional(*task_id_);
  }
  update.title = base::UTF16ToUTF8(task_title);

  // UI is updated via `OnSelectedTaskChanged()` once the update has been made
  // to the model.
  FocusModeController::Get()->tasks_model().UpdateTask(update);

  // When ChromeVox is on, we want to set the focus onto `complete_button_`
  // except for the case that we have already pressed TAB key to focus on
  // `deselect_button_`.
  if (IsSpokenFeedbackEnabled() &&
      (!prev_complete_button_visibility || !deselect_button_->HasFocus())) {
    complete_button_->RequestFocus();
  }
}

void FocusModeTaskView::PaintFocusRingAndUpdateStyle() {
  const bool is_active = textfield_->IsActive();
  if (is_active) {
    // `SystemTextfield::SetActive` will show focus ring when `textfield_` is
    // active. But in our case, we don't want the textfield to show the focus
    // ring except for when it's in selected state, but show its parent's focus
    // ring. Thus, we need to hide `textfield_`'s focus ring.
    if (!textfield_->show_selected()) {
      textfield_->SetShowFocusRing(false);
    }
  } else if (textfield_->HasFocus()) {
    // TODO(b/312226702): Remove the call for clearing the focus for the
    // `textfield_` after this bug resolved.
    // Commit changes if `textfield_` is inactive but still has the focus. This
    // case happens when the user types something in `textfield_` and clicks
    // outside of `textfield_` to commit changes.
    ClearFocusForTextfield(textfield_);
  }
  textfield_->UpdateElideBehavior(is_active);
  views::FocusRing::Get(textfield_container_)->SchedulePaint();
}

void FocusModeTaskView::OnCompleteTask() {
  FocusModeController::Get()->CompleteTask();
}

void FocusModeTaskView::OnDeselectButtonPressed() {
  OnClearTask();

  // When ChromeVox is on, we want to focus on the textfield_ after removing the
  // selected task.
  if (!IsSpokenFeedbackEnabled()) {
    return;
  }
  textfield_->RequestFocus();
  if (textfield_->HasFocus()) {
    textfield_->SetActive(true);
  }
}

void FocusModeTaskView::OnAddTaskButtonPressed() {
  if (auto* focus_manager = GetFocusManager()) {
    if (textfield_ != focus_manager->GetFocusedView()) {
      // When the `add_task_button_` is visible, it means this view isn't in
      // selected state. When clicking on the `add_task_button_`, if there is no
      // content for the `textfield_`, we should activate it and the cursor will
      // be shown on it; if the `textfield_` has some content, it means the user
      // is selecting the task, we shouldn't give the focus to the `textfield_`.
      // More info here b/343623327.
      if (textfield_->GetText().empty()) {
        GetFocusManager()->SetFocusedView(textfield_);
      }
    } else {
      // The `textfield_` may be inactive when it is focused, so we should
      // manually activate it in this case.
      textfield_->SetActive(true);
    }
  }
}

void FocusModeTaskView::UpdateStyle(bool show_selected_state,
                                    bool is_network_connected) {
  textfield_container_->SetBorder(views::CreateEmptyBorder(
      show_selected_state ? kSelectedStateBoxInsets
                          : kUnselectedStateBoxInsets));
  textfield_container_->SetBackground(
      show_selected_state ? nullptr
                          : views::CreateThemedRoundedRectBackground(
                                cros_tokens::kCrosSysInputFieldOnShaded,
                                kTextfieldCornerRadius));

  complete_button_->SetEnabled(is_network_connected);
  complete_button_->SetVisible(show_selected_state);
  if (show_selected_state) {
    complete_button_->GetViewAccessibility().SetDescription(
        textfield_->GetText());
  } else {
    complete_button_->GetViewAccessibility().SetDescription(
        std::u16string(),
        ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  }

  deselect_button_->SetVisible(show_selected_state);
  add_task_button_->SetVisible(!show_selected_state);

  // Note: don't show the carousel if we are editing a previously selected task.
  chip_carousel_->SetVisible(!show_selected_state &&
                             chip_carousel_->HasTasks());
  // Request a update for the scroll view and gradient for `chip_carousel_` when
  // it's visible.
  if (chip_carousel_->GetVisible()) {
    chip_carousel_->InvalidateLayout();
  }

  complete_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kRadioButtonUncheckedIcon,
                                     is_network_connected
                                         ? cros_tokens::kCrosSysPrimary
                                         : cros_tokens::kCrosSysDisabled,
                                     kIconSize));

  textfield_->set_show_selected_state(show_selected_state);
  textfield_->SetTooltipText(
      is_network_connected
          ? (show_selected_state ? textfield_->GetText() : std::u16string())
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_OFFLINE_TOOLTIP));
  textfield_->GetViewAccessibility().SetName(
      show_selected_state
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_SELECTED_ACCESSIBLE_NAME,
                textfield_->GetText())
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_UNSELECTED_ACCESSIBLE_NAME));
  textfield_->SetBorder(views::CreateEmptyBorder(
      show_selected_state ? kSelectedStateTextfieldInsets
                          : kUnselectedStateTextfieldInsets));
  textfield_->SetFontList(
      TypographyProvider::Get()
          ->ResolveTypographyToken(TypographyToken::kCrosBody2)
          .DeriveWithStyle(gfx::Font::FontStyle::NORMAL));
  textfield_->SetTextColorId(cros_tokens::kCrosSysOnSurface);
  textfield_->SchedulePaint();
}

BEGIN_METADATA(FocusModeTaskView)
END_METADATA

}  // namespace ash
