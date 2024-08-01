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
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace ash {

ClipboardHistoryLabel::ClipboardHistoryLabel(const std::u16string& text,
                                             gfx::ElideBehavior elide_behavior,
                                             size_t max_lines)
    : views::Label(text) {
  SetAutoColorReadabilityEnabled(false);
  SetElideBehavior(elide_behavior);
  SetEnabledColorId(cros_tokens::kTextColorPrimary);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetMaxLines(max_lines);

  // Available horizontal space for text item contents.
  const int contents_width =
      clipboard_history_util::GetPreferredItemViewWidth() -
      ClipboardHistoryViews::kContentsInsets.width();
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *this);

    // Reduce width to accommodate an icon when the refresh is enabled.
    const int label_width = contents_width -
                            ClipboardHistoryViews::kIconSize.width() -
                            ClipboardHistoryViews::kIconMargins.width();

    if (max_lines != 1u) {
      SetMultiLine(true);
      SizeToFit(label_width);
    } else {
      SetPreferredSize(
          gfx::Size(label_width, ClipboardHistoryViews::kLabelPreferredHeight));
    }
  } else {
    SetPreferredSize(gfx::Size(contents_width,
                               ClipboardHistoryViews::kLabelPreferredHeight));
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody1, *this);
  }
}

BEGIN_METADATA(ClipboardHistoryLabel)
END_METADATA

}  // namespace ash
