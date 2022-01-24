// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_label.h"

#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"

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
}

const char* ClipboardHistoryLabel::GetClassName() const {
  return "ClipboardHistoryLabel";
}

void ClipboardHistoryLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();

  // Use the light mode as default because the light mode is the default mode of
  // the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is fixed.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;

  SetEnabledColor(ash::AshColorProvider::Get()->GetContentLayerColor(
      ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
}

}  // namespace ash
