// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_task_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_chip_carousel.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr auto kEditTitleStateInsets = gfx::Insets::VH(8, 16);
constexpr auto kSavedTitleStateInsets = gfx::Insets::VH(4, 12);

}  // namespace

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
    owner_->SelectTask(textfield_->GetText());
  }

 private:
  const raw_ptr<SystemTextfield> textfield_;

  // The owning `FocusModeTaskView`.
  const raw_ptr<FocusModeTaskView> owner_;
};

FocusModeTaskView::FocusModeTaskView() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  auto* textfield_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  textfield_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  textfield_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  textfield_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  // TODO(b/306272008): Finalize the style, size and spacing for the radio
  // button and the close button. Wait for replacing the check icon with the one
  // from UX.
  radio_button_ =
      textfield_container->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&FocusModeTaskView::OnRadioButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kXSmall, &views::kRadioButtonNormalIcon,
          std::u16string(), /*is_togglable=*/true, /*has_border=*/false));
  radio_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_RADIO_BUTTON));
  radio_button_->SetToggledVectorIcon(kCheckIcon);

  task_title_ = FocusModeController::Get()->selected_task_title();

  textfield_ = textfield_container->AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium));
  textfield_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetShowBackground(true);
  textfield_->SetBackgroundColorId(cros_tokens::kCrosSysInputFieldOnShaded);
  textfield_->SetPlaceholderText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_TEXTFIELD_PLACEHOLDER));
  textfield_->SetPlaceholderTextColorId(cros_tokens::kCrosSysSecondary);
  textfield_->SetText(task_title_);

  deselect_button_ =
      textfield_container->AddChildView(std::make_unique<CloseButton>(
          base::BindRepeating(&FocusModeTaskView::OnDeselectButtonPressed,
                              base::Unretained(this)),
          CloseButton::Type::kMedium));

  chip_carousel_ =
      AddChildView(std::make_unique<FocusModeChipCarousel>(base::BindRepeating(
          &FocusModeTaskView::SelectTask, base::Unretained(this))));

  UpdateStyle(!task_title_.empty());

  textfield_controller_ =
      std::make_unique<TaskTextfieldController>(textfield_, this);
}

FocusModeTaskView::~FocusModeTaskView() = default;

void FocusModeTaskView::SelectTask(const std::u16string& task_title) {
  task_title_ = task_title;
  FocusModeController::Get()->set_selected_task_title(task_title_);
  UpdateStyle(/*show_selected_state=*/!task_title_.empty());
  // TODO(b/306271332): Call the tasks API to either save or update a task.
  // TODO(b/306271315): Save task info to user prefs.
}

void FocusModeTaskView::UpdateStyle(bool show_selected_state) {
  // If a task chip was selected, populate the textfield with its name and
  // unfocus the textfield.
  if (show_selected_state) {
    textfield_->SetText(task_title_);
    auto* focus_manager = textfield_->GetFocusManager();
    // If a task was selected from a chip, the textfield will still be focused.
    // Unfocus it in this case.
    if (focus_manager && focus_manager->GetFocusedView() == textfield_) {
      textfield_->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
      // If the textfield is focused, unfocusing it will end up calling this
      // method again.
      return;
    }
  }

  radio_button_->SetVisible(show_selected_state);
  deselect_button_->SetVisible(show_selected_state);
  chip_carousel_->SetVisible(!show_selected_state &&
                             chip_carousel_->HasTasks());

  // TODO(b/306272008): Update label color and add a strikethrough if it's
  // selected state.
  textfield_->SetBackgroundColorEnabled(!show_selected_state);
  textfield_->SetBorder(views::CreateEmptyBorder(
      show_selected_state ? kSavedTitleStateInsets : kEditTitleStateInsets));
  textfield_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      show_selected_state ? TypographyToken::kCrosButton2
                          : TypographyToken::kCrosButton1));
  textfield_->SchedulePaint();
}

}  // namespace ash
