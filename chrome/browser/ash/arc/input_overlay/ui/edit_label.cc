// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace arc::input_overlay {

EditLabel::EditLabel(DisplayOverlayController* controller,
                     Action* action,
                     size_t index)
    : views::LabelButton(),
      controller_(controller),
      action_(action),
      index_(index) {
  Init();
}

EditLabel::~EditLabel() = default;

void EditLabel::OnActionInputBindingUpdated() {
  if (action_->GetCurrentDisplayedInput().input_sources() ==
      InputSource::IS_NONE) {
    SetTextLabel(kUnknownBind);
  } else {
    const auto& keys = action_->GetCurrentDisplayedInput().keys();
    DCHECK(index_ < keys.size());
    SetTextLabel(GetDisplayText(keys[index_]));
  }
}

bool EditLabel::IsInputUnbound() {
  return GetText().compare(kUnknownBind) == 0;
}

void EditLabel::Init() {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetPreferredSize(gfx::Size(32, 32));
  SetAccessibilityProperties(ax::mojom::Role::kLabelText,
                             CalculateAccessibleName());
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(false);
  SetRequestFocusOnPress(true);
  SetAnimateOnStateChange(false);
  SetHotTracked(false);
  SetShowInkDropWhenHotTracked(false);
  SetHasInkDropActionOnClick(false);
  ash::bubble_utils::ApplyStyle(label(), ash::TypographyToken::kCrosHeadline1,
                                cros_tokens::kCrosSysOnPrimaryContainer);
  OnActionInputBindingUpdated();
}

void EditLabel::SetTextLabel(const std::u16string& text) {
  SetText(text);
  SetAccessibleName(CalculateAccessibleName());
  SetBackground(views::CreateThemedRoundedRectBackground(
      text == kUnknownBind ? cros_tokens::kCrosSysErrorHighlight
                           : cros_tokens::kCrosSysHighlightShape,
      /*radius=*/8));
  if (HasFocus()) {
    SetToFocused();
  } else {
    SetToDefault();
  }
}

void EditLabel::SetNameTagState(bool is_error,
                                const std::u16string& error_tooltip) {
  DCHECK(parent());
  auto* parent_view = static_cast<EditLabels*>(parent());
  parent_view->SetNameTagState(is_error, error_tooltip);
}

std::u16string EditLabel::CalculateAccessibleName() {
  return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEYMAPPING_KEY)
      .append(u" ")
      .append(GetDisplayTextAccessibleName(label()->GetText()));
}

void EditLabel::SetToDefault() {
  SetEnabledTextColorIds(IsInputUnbound()
                             ? cros_tokens::kCrosSysError
                             : cros_tokens::kCrosSysOnPrimaryContainer);
  SetBorder(nullptr);
}

void EditLabel::SetToFocused() {
  SetEnabledTextColorIds(IsInputUnbound() ? cros_tokens::kCrosSysError
                                          : cros_tokens::kCrosSysHighlightText);
  SetBorder(views::CreateThemedRoundedRectBorder(
      /*thickness=*/2, /*corner_radius=*/8, cros_tokens::kCrosSysPrimary));
}

void EditLabel::OnFocus() {
  LabelButton::OnFocus();
  SetToFocused();
}

void EditLabel::OnBlur() {
  LabelButton::OnBlur();
  SetToDefault();
  // Reset the error state if an reserved key was pressed.
  SetNameTagState(/*is_error=*/false, u"");
}

bool EditLabel::OnKeyPressed(const ui::KeyEvent& event) {
  auto code = event.code();
  std::u16string new_bind = GetDisplayText(code);
  if (GetText() == new_bind ||
      (!action_->support_modifier_key() &&
       ModifierDomCodeToEventFlag(code) != ui::EF_NONE) ||
      IsReservedDomCode(code)) {
    SetNameTagState(
        /*is_error=*/true,
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_RESERVED_KEYS));
    return false;
  }

  SetTextLabel(new_bind);

  std::unique_ptr<InputElement> input;
  switch (action_->GetType()) {
    case ActionType::TAP:
      input = InputElement::CreateActionTapKeyElement(code);
      break;
    case ActionType::MOVE: {
      const auto& input_binding = action_->GetCurrentDisplayedInput();
      auto new_keys = input_binding.keys();
      // If there is duplicated key in its own action, unset the key.
      const int unassigned_index = input_binding.GetIndexOfKey(code);
      if (unassigned_index != -1 && size_t(unassigned_index) != index_) {
        new_keys[unassigned_index] = ui::DomCode::NONE;
      }
      // Set the new key.
      new_keys[index_] = code;
      input = InputElement::CreateActionMoveKeyElement(new_keys);
      break;
    }
    default:
      NOTREACHED();
  }
  DCHECK(input);
  controller_->OnInputBindingChange(action_, std::move(input));
  return true;
}

BEGIN_METADATA(EditLabel, views::LabelButton)
END_METADATA

}  // namespace arc::input_overlay
