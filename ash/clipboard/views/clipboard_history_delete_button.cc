// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_delete_button.h"

#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"

namespace ash {
namespace {

// The size of the `DeleteButton`.
constexpr int kDeleteButtonSizeDip = 16;

}  // namespace

ClipboardHistoryDeleteButton::ClipboardHistoryDeleteButton(
    ClipboardHistoryItemView* listener)
    : views::ImageButton(base::BindRepeating(
          [](ClipboardHistoryItemView* item_view, const ui::Event& event) {
            item_view->HandleDeleteButtonPressEvent(event);
          },
          base::Unretained(listener))) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(base::ASCIIToUTF16(std::string(GetClassName())));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(kDeleteButtonSizeDip, kDeleteButtonSizeDip));
}

ClipboardHistoryDeleteButton::~ClipboardHistoryDeleteButton() = default;

const char* ClipboardHistoryDeleteButton::GetClassName() const {
  return "DeleteButton";
}

void ClipboardHistoryDeleteButton::OnThemeChanged() {
  // Use the light mode as default because the light mode is the default mode of
  // the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is fixed.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  views::ImageButton::OnThemeChanged();
  AshColorProvider::Get()->DecorateCloseButton(this, kDeleteButtonSizeDip,
                                               kCloseButtonIcon);
}
}  // namespace ash
