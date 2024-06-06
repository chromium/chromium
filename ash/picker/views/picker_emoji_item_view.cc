// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_item_view.h"

#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/views/picker_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerEmojiFontSize = 20;
const gfx::FontList kPickerEmojiFont({"Google Sans", "Roboto"},
                                     gfx::Font::NORMAL,
                                     kPickerEmojiFontSize,
                                     gfx::Font::Weight::NORMAL);

constexpr int kPickerEmojiItemCornerRadius = 4;

}  // namespace

PickerEmojiItemView::PickerEmojiItemView(
    SelectItemCallback select_item_callback,
    const std::u16string& emoji)
    : PickerItemView(std::move(select_item_callback)) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kPickerEmojiItemCornerRadius);
  SetProperty(views::kElementIdentifierKey, kPickerEmojiItemElementId);

  emoji_label_ = AddChildView(views::Builder<views::Label>()
                                  .SetText(emoji)
                                  .SetFontList(kPickerEmojiFont)
                                  .Build());
  GetViewAccessibility().SetName(*emoji_label_);
}

std::u16string_view PickerEmojiItemView::GetTextForTesting() const {
  return emoji_label_->GetText();
}

PickerEmojiItemView::~PickerEmojiItemView() = default;

BEGIN_METADATA(PickerEmojiItemView)
END_METADATA

}  // namespace ash
