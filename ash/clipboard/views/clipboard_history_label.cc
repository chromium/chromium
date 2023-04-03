// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_label.h"

#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {
ClipboardHistoryLabel::ClipboardHistoryLabel(const std::u16string& text)
    : views::Label(text) {
  SetPreferredSize(
      gfx::Size(INT_MAX, ClipboardHistoryViews::kLabelPreferredHeight));
  if (chromeos::features::IsJellyEnabled()) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody1, *this);
  } else {
    SetFontList(views::style::GetFont(views::style::CONTEXT_TOUCH_MENU,
                                      views::style::STYLE_PRIMARY));
  }
  SetMultiLine(false);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColorId(cros_tokens::kTextColorPrimary);
}

BEGIN_METADATA(ClipboardHistoryLabel, views::Label)
END_METADATA

}  // namespace ash
