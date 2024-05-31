// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/login_textfield.h"

#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

namespace {

// Spacing between glyphs, used when password is in hidden state.
constexpr int kPasswordGlyphSpacing = 6;

// Size (width/height) of the different icons belonging to the password row
// (the display password icon and the caps lock icon).
constexpr int kIconSizeDp = 20;

// The max width of the LoginTextfield.
constexpr int kLogintTextfieldMaxWidthDp = 293;

}

LoginTextfield::LoginTextfield() : SystemTextfield(Type::kMedium) {
  const gfx::FontList font_list =
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kCrosBody2);

  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  set_placeholder_font_list(font_list);
  SetFontList(font_list);
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
}

LoginTextfield::~LoginTextfield() = default;

void LoginTextfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (!GetText().empty()) {
    SelectAll(/*reversed=*/false);
  }
}

void LoginTextfield::OnBlur() {
  SystemTextfield::OnBlur();
  CHECK(delegate_);
  delegate_->OnTextfieldBlur();
}

void LoginTextfield::OnFocus() {
  SystemTextfield::OnFocus();
  CHECK(delegate_);
  delegate_->OnTextfieldFocus();
}

gfx::Size LoginTextfield::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kLogintTextfieldMaxWidthDp, kIconSizeDp);
}

void LoginTextfield::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

BEGIN_METADATA(LoginTextfield)
END_METADATA

}  // namespace ash
