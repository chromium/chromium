// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_label.h"

#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {
ClipboardHistoryLabel::ClipboardHistoryLabel(const std::u16string& text)
    : views::Label(text) {
  SetPreferredSize(
      gfx::Size(INT_MAX, ClipboardHistoryViews::kLabelPreferredHeight));
  SetFontList(views::style::GetFont(views::style::CONTEXT_TOUCH_MENU,
                                    views::style::STYLE_PRIMARY));
  SetMultiLine(false);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColorId(cros_tokens::kTextColorPrimary);
}

const char* ClipboardHistoryLabel::GetClassName() const {
  return "ClipboardHistoryLabel";
}

}  // namespace ash
