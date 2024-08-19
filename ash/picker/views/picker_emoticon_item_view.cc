// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoticon_item_view.h"

#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/views/picker_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerEmoticonFontSize = 14;
const gfx::FontList kPickerEmoticonFont({"Google Sans", "Roboto"},
                                        gfx::Font::NORMAL,
                                        kPickerEmoticonFontSize,
                                        gfx::Font::Weight::NORMAL);

constexpr auto kPickerEmoticonItemMargins = gfx::Insets::VH(0, 6);

constexpr int kPickerEmoticonItemCornerRadius = 4;

}  // namespace

PickerEmoticonItemView::PickerEmoticonItemView(
    SelectItemCallback select_item_callback,
    const std::u16string& emoticon)
    : PickerItemView(std::move(select_item_callback)) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kPickerEmoticonItemCornerRadius);
  SetProperty(views::kElementIdentifierKey, kPickerEmojiItemElementId);

  emoticon_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetText(emoticon)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(kPickerEmoticonFont)
          .SetBorder(views::CreateEmptyBorder(kPickerEmoticonItemMargins))
          .Build());
  GetViewAccessibility().SetName(*emoticon_label_);
}

PickerEmoticonItemView::~PickerEmoticonItemView() = default;

std::u16string_view PickerEmoticonItemView::GetTextForTesting() const {
  return emoticon_label_->GetText();
}

BEGIN_METADATA(PickerEmoticonItemView)
END_METADATA

}  // namespace ash
