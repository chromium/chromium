// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace arc::input_overlay {

EditLabel::EditLabel(Action* action, size_t index)
    : views::LabelButton(), action_(action), index_(index) {
  Init();
}

EditLabel::~EditLabel() = default;

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

  const auto& keys = action_->GetCurrentDisplayedInput().keys();
  if (index_ >= keys.size() || keys[index_] == ui::DomCode::NONE) {
    SetText(kUnknownBind);
    SetToUnbound();
  } else {
    SetText(GetDisplayText(keys[index_]));
    SetToDefault();
  }
  SetAccessibleName(CalculateAccessibleName());
}

std::u16string EditLabel::CalculateAccessibleName() {
  return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEYMAPPING_KEY)
      .append(u" ")
      .append(GetDisplayTextAccessibleName(label()->GetText()));
}

bool EditLabel::IsInputUnbound() {
  return GetText().compare(kUnknownBind) == 0;
}

void EditLabel::SetToDefault() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHighlightShape, /*corner_radius=*/8));
  ash::bubble_utils::ApplyStyle(label(), ash::TypographyToken::kLegacyHeadline1,
                                cros_tokens::kCrosSysOnPrimaryContainer);
  SetBorder(nullptr);
}

void EditLabel::SetToFocused() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHighlightShape, /*corner_radius=*/8));
  ash::bubble_utils::ApplyStyle(label(), ash::TypographyToken::kLegacyHeadline1,
                                cros_tokens::kCrosSysHighlightText);
  SetBorder(views::CreateThemedRoundedRectBorder(
      /*thickness=*/2, /*corner_radius=*/8, cros_tokens::kCrosSysPrimary));
}

void EditLabel::SetToUnbound() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosRefError30, /*corner_radius=*/8));
  ash::bubble_utils::ApplyStyle(label(), ash::TypographyToken::kLegacyHeadline1,
                                cros_tokens::kCrosRefError0);
  SetBorder(nullptr);
}

void EditLabel::OnFocus() {
  LabelButton::OnFocus();

  if (IsInputUnbound()) {
    SetToUnbound();
  } else {
    SetToFocused();
  }
}

void EditLabel::OnBlur() {
  LabelButton::OnBlur();

  if (IsInputUnbound()) {
    SetToUnbound();
  } else {
    SetToDefault();
  }
}

BEGIN_METADATA(EditLabel, views::LabelButton)
END_METADATA

}  // namespace arc::input_overlay
