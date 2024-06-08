// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_task_view.h"

#include "ash/api/tasks/tasks_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
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
    show_selected_state_ = show_selected_state;
  }

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

  // SystemTextfield:
  void OnFocus() override {
    if (show_selected_state_) {
      // If we are in a selected state, we want to make the textfield focused
      // but not active, so that we can allow the user to press the `Enter` key
      // to activate the textfield. Thus, we only need to show its focus ring.
      SetShowFocusRing(true);
      return;
    }

    SystemTextfield::OnFocus();
  }

  void OnBlur() override {
    SystemTextfield::OnBlur();
    // Remove the focus ring for the state that the textfield was focused but
    // not active.
    if (show_selected_state_) {
      SetShowFocusRing(false);
    }
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
    if (key_event.type() == ui::ET_KEY_PRESSED &&
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
    owner_->AddOrUpdateTask(textfield_->GetText());
  }

 private:
  const raw_ptr<SystemTextfield> textfield_;

  // The owning `FocusModeTaskView`.
  const raw_ptr<FocusModeTaskView> owner_;
};

//---------------------------------------------------------------------
// FocusModeTaskView:

FocusModeTaskView::FocusModeTaskView(bool is_network_connected) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  textfield_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  textfield_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  textfield_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  textfield_container_->SetProperty(views::kBoxLayoutFlexKey,
                                    views::BoxLayoutFlexSpecification());
  radio_button_ =
      textfield_container_->AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&FocusModeTaskView::OnCompleteTask,
                              base::Unretained(this), /*update=*/true)));
  const std::u16string radio_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_VIEW_RADIO_BUTTON);
  radio_button_->GetViewAccessibility().SetName(radio_text);
  radio_button_->SetTooltipText(radio_text);

  views::FocusRing::Install(radio_button_);
  views::FocusRing::Get(radio_button_)
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
  add_task_button_->SetFocusBehavior(View::FocusBehavior::NEVER);
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
  textfield_container_focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](const TaskTextfield* textfield, const views::View* view) {
        return textfield && textfield->IsActive();
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

  chip_carousel_ =
      AddChildView(std::make_unique<FocusModeChipCarousel>(base::BindRepeating(
          &FocusModeTaskView::OnTaskSelected, base::Unretained(this))));
  auto* controller = FocusModeController::Get();
  const bool has_selected_task = controller->HasSelectedTask();
  const std::string& selected_task_title = controller->selected_task_title();
  if (has_selected_task) {
    // There is a chance that we have a selected task but the title isn't
    // updated yet, since we do not save that to user prefs.
    if (!selected_task_title.empty()) {
      task_title_ = base::UTF8ToUTF16(selected_task_title);
    }

    if (is_network_connected) {
      // Fetch the selected task to verify if it is still in the uncompleted
      // state.
      controller->tasks_provider().GetTask(
          controller->selected_task_list_id(), controller->selected_task_id(),
          base::BindOnce(&FocusModeTaskView::OnTaskFetched,
                         weak_factory_.GetWeakPtr()));
    }
  } else if (is_network_connected) {
    controller->tasks_provider().GetSortedTaskList(base::BindOnce(
        &FocusModeTaskView::OnTasksFetched, weak_factory_.GetWeakPtr()));
  }

  UpdateStyle(/*show_selected_state=*/(has_selected_task &&
                                       !selected_task_title.empty()),
              /*is_network_connected=*/is_network_connected);

  textfield_controller_ =
      std::make_unique<TaskTextfieldController>(textfield_, this);
}

FocusModeTaskView::~FocusModeTaskView() = default;

void FocusModeTaskView::AddOrUpdateTask(const std::u16string& task_title) {
  if (task_title.empty()) {
    OnClearTask();
    return;
  }

  auto* controller = FocusModeController::Get();
  if (controller->HasSelectedTask()) {
    controller->tasks_provider().UpdateTask(
        controller->selected_task_list_id(), controller->selected_task_id(),
        base::UTF16ToUTF8(task_title), /*completed=*/false,
        base::BindOnce(&FocusModeTaskView::OnTaskSelected,
                       weak_factory_.GetWeakPtr()));
  } else {
    controller->tasks_provider().AddTask(
        base::UTF16ToUTF8(task_title),
        base::BindOnce(&FocusModeTaskView::OnTaskSelected,
                       weak_factory_.GetWeakPtr()));
  }
}

void FocusModeTaskView::OnTaskSelected(const FocusModeTask& task_entry) {
  if (task_entry.task_id.empty() || task_entry.title.empty()) {
    OnClearTask();
    return;
  }

  task_title_ = base::UTF8ToUTF16(task_entry.title);
  textfield_->SetText(task_title_);
  FocusModeController::Get()->SetSelectedTask(task_entry);
  UpdateStyle(/*show_selected_state=*/true);
}

void FocusModeTaskView::OnClearTask() {
  task_title_.clear();
  textfield_->SetText(std::u16string());
  auto* controller = FocusModeController::Get();
  controller->SetSelectedTask({});
  // Only update `chip_carousel_` when it's invisible to avoid the crash when
  // moving focus to it by tabbing from an empty text of `textfield_` to the
  // `chip_carousel_`.
  if (!chip_carousel_->GetVisible()) {
    controller->tasks_provider().GetSortedTaskList(base::BindOnce(
        &FocusModeTaskView::OnTasksFetched, weak_factory_.GetWeakPtr()));
  }
  UpdateStyle(/*show_selected_state=*/false);
}

void FocusModeTaskView::PaintFocusRingAndUpdateStyle() {
  const bool is_active = textfield_->IsActive();
  if (is_active) {
    UpdateStyle(false);
    // `SystemTextfield::SetActive` will show focus ring when `textfield_` is
    // active. But in our case, we don't want the textfield to show the focus
    // ring, but show its parent focus ring. Thus, we need to hide
    // `textfield_`'s focus ring.
    textfield_->SetShowFocusRing(false);
  } else if (textfield_->HasFocus()) {
    // TODO(b/312226702): Remove the call for clearing the focus for the
    // `textfield_` after this bug resolved.
    // Commit changes if `textfield_` is inactive but still has the focus. This
    // case happens when the user types something in `textfield_` and clicks
    // outside of `textfield_` to commit changes.
    ClearFocusForTextfield(textfield_);
  }
  textfield_->UpdateElideBehavior(is_active);
}

void FocusModeTaskView::OnCompleteTask(bool update) {
  FocusModeController::Get()->CompleteTask(update);

  // Not having a populated `task_title_` means that the UI is not in the
  // selected state, so we don't need to update the styling and can immediately
  // clear the task. This can only happen when we initialize the focus panel for
  // the first time and we have a selected task without a title, since we do not
  // save that to user prefs.
  if (task_title_.empty()) {
    OnClearTask();
    return;
  }

  radio_button_->SetEnabled(false);
  radio_button_->SetImageModel(
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

void FocusModeTaskView::OnDeselectButtonPressed() {
  OnClearTask();
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

void FocusModeTaskView::OnTasksFetched(
    const std::vector<FocusModeTask>& tasks) {
  chip_carousel_->SetTasks(tasks);
  chip_carousel_->SetVisible(!tasks.empty());
}

void FocusModeTaskView::OnTaskFetched(const FocusModeTask& task_entry) {
  // If the selected task could not be found, then an error has occurred.
  if (task_entry.task_id.empty()) {
    return;
  }

  if (task_entry.completed) {
    OnCompleteTask(/*update=*/false);
  } else {
    OnTaskSelected(task_entry);
  }
}

void FocusModeTaskView::UpdateStyle(bool show_selected_state,
                                    bool is_network_connected) {
  textfield_->SetText(task_title_);
  // Unfocus the textfield if a task is selected.
  if (show_selected_state) {
    auto* focus_manager = textfield_->GetFocusManager();
    // If a task was selected from a chip, the textfield will still be focused.
    // Unfocus it in this case.
    if (focus_manager && focus_manager->GetFocusedView() == textfield_) {
      textfield_->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
      // If the textfield is focused, unfocusing it will end up calling this
      // method again.
      return;
    }
  } else {
    // Clear `task_title_` if no task is selected so that if a list of tasks is
    // returned while editing the textfield, the chip carousel is shown.
    task_title_.clear();
  }

  textfield_container_->SetBorder(views::CreateEmptyBorder(
      show_selected_state ? kSelectedStateBoxInsets
                          : kUnselectedStateBoxInsets));
  textfield_container_->SetBackground(
      show_selected_state ? nullptr
                          : views::CreateThemedRoundedRectBackground(
                                cros_tokens::kCrosSysInputFieldOnShaded,
                                kTextfieldCornerRadius));

  radio_button_->SetEnabled(is_network_connected);
  radio_button_->SetVisible(show_selected_state);
  deselect_button_->SetVisible(show_selected_state);
  add_task_button_->SetVisible(!show_selected_state);
  // Note: don't show the carousel if we are editing a previously selected task.
  chip_carousel_->SetVisible(!show_selected_state &&
                             !FocusModeController::Get()->HasSelectedTask() &&
                             chip_carousel_->HasTasks());

  radio_button_->SetImageModel(
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
                task_title_)
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
