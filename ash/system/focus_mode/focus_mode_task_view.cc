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
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kIconSize = 20;
constexpr auto kSelectedStateBoxInsets = gfx::Insets::TLBR(8, 0, 0, 0);
constexpr auto kSelectedStateTextfieldInsets = gfx::Insets::TLBR(0, 16, 0, 12);

constexpr int kUnselectedStateBoxCornerRadius = 4;
constexpr auto kUnselectedStateBoxInsets = gfx::Insets::TLBR(4, 8, 4, 16);
constexpr auto kUnselectedStateTextfieldInsets = gfx::Insets::TLBR(0, 8, 0, 0);

constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);

}  // namespace

//---------------------------------------------------------------------
// FocusModeTaskView::TaskTextfield:

class FocusModeTaskView::TaskTextfield : public SystemTextfield {
 public:
  // The `kMedium` type of `SystemTextfield` has a 20px font size and a 28px
  // container height.
  TaskTextfield() : SystemTextfield(SystemTextfield::Type::kMedium) {
    // Don't show focus ring for the textfield.
    views::FocusRing::Get(this)->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return false; }));

    // `SystemTextfield` separates the "focused" and "active" states. The
    // textfield can be focused but inactive. For example, while editing, if the
    // user presses Enter, it will commit the changes and deactivate the
    // textfield, but the textfield is also focused such that we can re-activate
    // it by pressing Enter agagin. In focused mode case, we only want to show
    // the focus ring when the textfield is active. Thus, we will paint the
    // focus ring of `parent()` each time on the active state changed.
    SetActiveStateChangedCallback(base::BindRepeating(
        &FocusModeTaskView::TaskTextfield::PaintParentFocusRing,
        base::Unretained(this)));
  }
  TaskTextfield(const TaskTextfield&) = delete;
  TaskTextfield& operator=(const TaskTextfield&) = delete;
  ~TaskTextfield() override = default;

  void set_show_tooltip(bool show_tooltip) { show_tooltip_ = show_tooltip; }

  void SetElideTail(bool elide_tail) {
    GetRenderText()->SetElideBehavior(elide_tail ? gfx::ELIDE_TAIL
                                                 : gfx::NO_ELIDE);
  }

  // views::View:
  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return show_tooltip_ ? GetText() : std::u16string();
  }

 private:
  void PaintParentFocusRing() {
    views::FocusRing::Get(parent())->SchedulePaint();
  }

  // Indicates if the textfield should should show the tooltip.
  bool show_tooltip_ = false;
};

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

  // views::SystemTextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::ET_KEY_PRESSED &&
        key_event.key_code() == ui::VKEY_RETURN) {
      views::FocusManager* focus_manager =
          sender->GetWidget()->GetFocusManager();
      focus_manager->ClearFocus();

      // Avoid having the focus restored to the same view when the parent view
      // is refocused.
      focus_manager->SetStoredFocusView(nullptr);
      return true;
    }

    // TODO(b/306271947): Verify the `ESC` key to restore the text when the user
    // edits a task.
    return SystemTextfieldController::HandleKeyEvent(sender, key_event);
  }

  // views::ViewObserver:
  void OnViewFocused(View* observed_view) override {
    owner_->UpdateStyle(/*show_selected_state=*/false);
  }

  void OnViewBlurred(views::View* view) override {
    owner_->AddTask(textfield_->GetText());
  }

 private:
  const raw_ptr<SystemTextfield> textfield_;

  // The owning `FocusModeTaskView`.
  const raw_ptr<FocusModeTaskView> owner_;
};

//---------------------------------------------------------------------
// FocusModeTaskView:

FocusModeTaskView::FocusModeTaskView() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  textfield_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  textfield_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  textfield_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  textfield_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  radio_button_ = textfield_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &FocusModeTaskView::OnCompleteTask, base::Unretained(this))));
  radio_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_RADIO_BUTTON));

  add_task_button_ = textfield_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &FocusModeTaskView::OnAddTaskButtonPressed, base::Unretained(this))));
  add_task_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kGlanceablesTasksAddNewTaskIcon,
                                     cros_tokens::kCrosSysSecondary,
                                     kIconSize));
  add_task_button_->SetFocusBehavior(View::FocusBehavior::NEVER);

  auto* focus_mode_controller = FocusModeController::Get();
  task_title_ = focus_mode_controller->selected_task_title();
  textfield_ =
      textfield_container_->AddChildView(std::make_unique<TaskTextfield>());
  textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetBackgroundColorEnabled(false);
  textfield_->SetPlaceholderText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetPlaceholderTextColorId(cros_tokens::kCrosSysSecondary);
  textfield_container_->SetFlexForView(textfield_, 1);
  // We only show `textfield_container_`'s focus ring when the textfield is
  // active.
  views::FocusRing::Install(textfield_container_);
  auto* textfield_container_focus_ring =
      views::FocusRing::Get(textfield_container_);
  textfield_container_focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
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
                                     cros_tokens::kCrosSysSecondary,
                                     kIconSize));
  deselect_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_DESELECT_BUTTON));

  chip_carousel_ =
      AddChildView(std::make_unique<FocusModeChipCarousel>(base::BindRepeating(
          &FocusModeTaskView::SelectTask, base::Unretained(this))));
  chip_carousel_->SetTasks(
      focus_mode_controller->tasks_provider().GetTaskList());

  UpdateStyle(!task_title_.empty());

  textfield_controller_ =
      std::make_unique<TaskTextfieldController>(textfield_, this);
}

FocusModeTaskView::~FocusModeTaskView() = default;

void FocusModeTaskView::AddTask(const std::u16string& task_title) {
  if (task_title.empty()) {
    return;
  }

  // If a task is already selected, edit it. Add it otherwise.
  auto* controller = FocusModeController::Get();
  if (!controller->selected_task_title().empty()) {
    // TODO(b/306271947): Edit an existing task
  } else {
    controller->tasks_provider().CreateTask(base::UTF16ToUTF8(task_title));
  }

  task_title_ = task_title;
  controller->set_selected_task_title(task_title_);
  UpdateStyle(/*show_selected_state=*/true);
}

void FocusModeTaskView::SelectTask(const api::Task* task) {
  task_title_ = base::UTF8ToUTF16(task->title);
  textfield_->SetText(task_title_);
  FocusModeController::Get()->set_selected_task_title(task_title_);
  UpdateStyle(/*show_selected_state=*/!task_title_.empty());
  // TODO(b/306271332): Call the tasks API to either save or update a task.
  // TODO(b/306271315): Save task info to user prefs.
}

void FocusModeTaskView::OnCompleteTask() {
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
  task_title_.clear();
  FocusModeController::Get()->set_selected_task_title(task_title_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FocusModeTaskView::UpdateStyle,
                     weak_factory_.GetWeakPtr(), false),
      kStartAnimationDelay);
}

void FocusModeTaskView::OnDeselectButtonPressed() {
  task_title_.clear();
  FocusModeController::Get()->set_selected_task_title(task_title_);
  UpdateStyle(/*show_selected_state=*/false);
}

void FocusModeTaskView::OnAddTaskButtonPressed() {
  if (auto* focus_manager = GetFocusManager()) {
    if (textfield_ != focus_manager->GetFocusedView()) {
      GetFocusManager()->SetFocusedView(textfield_);
    } else {
      // The `textfield_` may be inactive when it is focused, so we should
      // manually activate it in this case.
      textfield_->SetActive(true);
    }
  }
}

void FocusModeTaskView::UpdateStyle(bool show_selected_state) {
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
                                kUnselectedStateBoxCornerRadius));

  radio_button_->SetEnabled(true);
  radio_button_->SetVisible(show_selected_state);
  deselect_button_->SetVisible(show_selected_state);
  add_task_button_->SetVisible(!show_selected_state);
  chip_carousel_->SetVisible(!show_selected_state &&
                             chip_carousel_->HasTasks());

  radio_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kRadioButtonUncheckedIcon,
                                     cros_tokens::kCrosSysPrimary, kIconSize));

  textfield_->set_show_tooltip(/*show_tooltip=*/show_selected_state);
  textfield_->SetElideTail(/*elide_tail=*/show_selected_state);
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

}  // namespace ash
