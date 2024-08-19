// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_symbol_item_view.h"

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

constexpr int kPickerSymbolFontSize = 16;
const gfx::FontList kPickerSymbolFont({"Google Sans", "Roboto"},
                                      gfx::Font::NORMAL,
                                      kPickerSymbolFontSize,
                                      gfx::Font::Weight::NORMAL);

constexpr int kPickerSymbolItemCornerRadius = 4;

}  // namespace

PickerSymbolItemView::PickerSymbolItemView(
    SelectItemCallback select_item_callback,
    const std::u16string& symbol)
    : PickerItemView(std::move(select_item_callback)) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kPickerSymbolItemCornerRadius);
  SetProperty(views::kElementIdentifierKey, kPickerEmojiItemElementId);

  symbol_label_ =
      AddChildView(views::Builder<views::Label>()
                       .SetText(symbol)
                       .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                       .SetFontList(kPickerSymbolFont)
                       .Build());
  GetViewAccessibility().SetName(*symbol_label_);
}

PickerSymbolItemView::~PickerSymbolItemView() = default;

std::u16string_view PickerSymbolItemView::GetTextForTesting() const {
  return symbol_label_->GetText();
}

BEGIN_METADATA(PickerSymbolItemView)
END_METADATA

}  // namespace ash
