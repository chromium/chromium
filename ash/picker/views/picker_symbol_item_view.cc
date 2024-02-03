// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_symbol_item_view.h"

#include <string>
#include <utility>

#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

constexpr int kPickerSymbolFontSize = 16;
const gfx::FontList kPickerSymbolFont({"Google Sans", "Roboto"},
                                      gfx::Font::NORMAL,
                                      kPickerSymbolFontSize,
                                      gfx::Font::Weight::NORMAL);

constexpr auto kPickerSymbolItemCornerRadius = gfx::RoundedCornersF(4);

}  // namespace

PickerSymbolItemView::PickerSymbolItemView(
    views::Button::PressedCallback callback,
    const std::u16string& symbol)
    : views::Button(std::move(callback)) {
  SetUseDefaultFillLayout(true);

  symbol_label_ =
      AddChildView(views::Builder<views::Label>()
                       .SetText(symbol)
                       .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                       .SetFontList(kPickerSymbolFont)
                       .Build());
  SetAccessibleName(symbol_label_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, kPickerSymbolItemCornerRadius);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

PickerSymbolItemView::~PickerSymbolItemView() = default;

BEGIN_METADATA(PickerSymbolItemView)
END_METADATA

}  // namespace ash
