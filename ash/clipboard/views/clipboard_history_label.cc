// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_label.h"

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {
ClipboardHistoryLabel::ClipboardHistoryLabel(const std::u16string& text)
    : views::Label(text) {
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColorId(cros_tokens::kTextColorPrimary);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Available horizontal space for text item contents.
  const int contents_width =
      clipboard_history_util::GetPreferredItemViewWidth() -
      ClipboardHistoryViews::kContentsInsets.width();
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *this);
    SetMultiLine(true);
    SetMaxLines(ClipboardHistoryViews::kTextItemMaxLines);
    // Reduce width to accommodate an icon when the refresh is enabled.
    SizeToFit(contents_width - ClipboardHistoryViews::kIconSize.width() -
              ClipboardHistoryViews::kIconMargins.width());
  } else {
    SetPreferredSize(gfx::Size(contents_width,
                               ClipboardHistoryViews::kLabelPreferredHeight));
    if (chromeos::features::IsJellyEnabled()) {
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody1, *this);
    } else {
      SetFontList(views::style::GetFont(views::style::CONTEXT_TOUCH_MENU,
                                        views::style::STYLE_PRIMARY));
    }
  }
}

BEGIN_METADATA(ClipboardHistoryLabel, views::Label)
END_METADATA

}  // namespace ash
