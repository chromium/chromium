// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_textfield.h"

#include <string>

#include "ash/auth/views/auth_textfield_timer.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/observer_list_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Spacing between glyphs, used when password is in hidden state.
constexpr int kPasswordGlyphSpacing = 6;

// Size (width/height) of the different icons belonging to the password row
// (the display password icon and the caps lock icon).
constexpr int kIconSizeDp = 20;

// The max width of the AuthTextfield.
constexpr int kAuthTextfieldMaxWidthDp = 216;

// Font type for text.
constexpr TypographyToken kTextFont = TypographyToken::kCrosBody2;

// Font type for hidden text.
constexpr TypographyToken kHiddenTextFont = TypographyToken::kCrosDisplay5;
}

AuthTextfield::AuthTextfield(AuthType auth_type)
    : SystemTextfield(Type::kMedium),
      SystemTextfieldController(this),
      auth_type_(auth_type) {

  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_placeholder_font_list(
      ash::TypographyProvider::Get()->ResolveTypographyToken(kTextFont));
  SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(kHiddenTextFont));
  SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
  // Remove focus ring to remain consistent with other implementations of
  // login input fields.
  views::FocusRing::Remove(this);

  // Don't show background.
  SetShowBackground(false);
  SetBackgroundEnabled(false);
  SetBackground(nullptr);

  // Remove the border.
  SetBorder(nullptr);

  // Set the text colors.
  SetTextColorId(cros_tokens::kCrosSysOnSurface);
  SetPlaceholderTextColorId(cros_tokens::kCrosSysDisabled);

  // Set Accessible name
  if (auth_type_ == AuthType::kPassword) {
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_AUTH_TEXTFIELD_PASSWORD_ACCESSIBLE_NAME));
    SetPlaceholderText(l10n_util::GetStringUTF16(
        IDS_ASH_IN_SESSION_AUTH_PASSWORD_PLACEHOLDER));
  } else {
    CHECK_EQ(auth_type_, AuthType::kPin);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ASH_AUTH_TEXTFIELD_PIN_ACCESSIBLE_NAME));
    SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_PLACEHOLDER));
  }
}

AuthTextfield::~AuthTextfield() = default;

void AuthTextfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (!GetText().empty()) {
    SelectAll(/*reversed=*/false);
  }
}

void AuthTextfield::OnBlur() {
  SystemTextfield::OnBlur();
  for (auto& observer : observers_) {
    observer.OnTextfieldBlur();
  }
}

void AuthTextfield::OnFocus() {
  SystemTextfield::OnFocus();
  for (auto& observer : observers_) {
    observer.OnTextfieldFocus();
  }
}

ui::TextInputMode AuthTextfield::GetTextInputMode() const {
  if (auth_type_ == AuthType::kPin) {
    return ui::TextInputMode::TEXT_INPUT_MODE_NUMERIC;
  }
  return ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT;
}

bool AuthTextfield::ShouldDoLearning() {
  return false;
}

bool AuthTextfield::HandleKeyEvent(views::Textfield* sender,
                                    const ui::KeyEvent& key_event) {
  CHECK_EQ(sender, this);

  if (GetReadOnly()) {
    return false;
  }

  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  const ui::KeyboardCode key_code = key_event.key_code();

  if (key_code == ui::VKEY_RETURN) {
    if (!GetText().empty()) {
      for (auto& observer : observers_) {
        observer.OnSubmit();
      }
    }
    return true;
  }

  if (key_code == ui::VKEY_ESCAPE) {
    for (auto& observer : observers_) {
      observer.OnEscape();
    }
    return true;
  }

  if (auth_type_ == AuthType::kPassword) {
    return SystemTextfieldController::HandleKeyEvent(sender, key_event);
  }

  CHECK_EQ(auth_type_, AuthType::kPin);

  // Default handling for events with Alt modifier like spoken feedback.
  if (key_event.IsAltDown()) {
    return false;
  }

  // Default handling for events with Control modifier like sign out.
  if (key_event.IsControlDown()) {
    return false;
  }

  // All key pressed events not handled below are ignored.
  if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB) {
    // Allow using tab for keyboard navigation.
    return false;
  } else if (key_code == ui::VKEY_PROCESSKEY) {
    // Default handling for keyboard events that are not generated by physical
    // key press. This can happen, for example, when virtual keyboard button
    // is pressed.
    return false;
  } else if (key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9) {
    InsertDigit(key_code - ui::VKEY_0);
  } else if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9) {
    InsertDigit(key_code - ui::VKEY_NUMPAD0);
  } else if (key_code == ui::VKEY_BACK) {
    return false;
  } else if (key_code == ui::VKEY_DELETE) {
    return false;
  } else if (key_code == ui::VKEY_LEFT) {
    return false;
  } else if (key_code == ui::VKEY_RIGHT) {
    return false;
  }

  return true;
}

void AuthTextfield::ContentsChanged(Textfield* sender,
                                     const std::u16string& new_contents) {
  for (auto& observer : observers_) {
    observer.OnContentsChanged(new_contents);
  }
}

gfx::Size AuthTextfield::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kAuthTextfieldMaxWidthDp, kIconSizeDp);
}

void AuthTextfield::Reset() {
  if (!GetText().empty()) {
    SetText(std::u16string());
    for (auto& observer : observers_) {
      observer.OnContentsChanged(GetText());
    }
  }
  HideText();
  ClearEditHistory();
}

void AuthTextfield::InsertDigit(int digit) {
  CHECK(auth_type_ == AuthType::kPin);
  CHECK(0 <= digit && digit <= 9);
  if (GetReadOnly()) {
    return;
  }

  if (!HasFocus()) {
    // RequestFocus on textfield to activate cursor.
    RequestFocus();
  }
  InsertOrReplaceText(base::NumberToString16(digit));
}

void AuthTextfield::Backspace() {
  // Instead of just adjusting textfield_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  if (GetReadOnly()) {
    return;
  }

  if (!HasFocus()) {
    // RequestFocus on textfield to activate cursor.
    RequestFocus();
  }

  auto* view = static_cast<views::View*>(this);
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
}

void AuthTextfield::SetTextVisible(bool visible) {
  if (visible) {
    ShowText();
  } else {
    HideText();
  }
}

void AuthTextfield::ShowText() {
  if (IsTextVisible()) {
    return;
  }
  SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(kTextFont));
  switch (auth_type_) {
    case AuthType::kPassword:
      SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);
      break;
    case AuthType::kPin:
      SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
      break;
  }
  for (auto& observer : observers_) {
    observer.OnTextVisibleChanged(true);
  }
}

void AuthTextfield::HideText() {
  if (!IsTextVisible()) {
    return;
  }
  SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(kHiddenTextFont));
  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  for (auto& observer : observers_) {
    observer.OnTextVisibleChanged(false);
  }
}

bool AuthTextfield::IsTextVisible() const {
  return GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;
}

void AuthTextfield::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthTextfield::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthTextfield::ApplyTimerLogic() {
  CHECK_EQ(timer_logic_.get(), nullptr);
  timer_logic_ = std::make_unique<AuthTextfieldTimer>(this);
}

void AuthTextfield::ResetTimerLogic() {
  CHECK(timer_logic_.get());
  timer_logic_.reset();
}

BEGIN_METADATA(AuthTextfield)
END_METADATA

}  // namespace ash
