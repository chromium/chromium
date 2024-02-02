// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoticon_item_view.h"

#include <string>
#include <utility>

#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

constexpr int kPickerEmoticonFontSize = 14;
const gfx::FontList kPickerEmoticonFont({"Google Sans", "Roboto"},
                                        gfx::Font::NORMAL,
                                        kPickerEmoticonFontSize,
                                        gfx::Font::Weight::NORMAL);

constexpr auto kPickerEmoticonItemMargins = gfx::Insets::VH(0, 6);

constexpr auto kPickerEmoticonItemCornerRadius = gfx::RoundedCornersF(4);

}  // namespace

PickerEmoticonItemView::PickerEmoticonItemView(
    views::Button::PressedCallback callback,
    const std::u16string& emoticon)
    : views::Button(std::move(callback)) {
  SetUseDefaultFillLayout(true);

  emoticon_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetText(emoticon)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(kPickerEmoticonFont)
          .SetBorder(views::CreateEmptyBorder(kPickerEmoticonItemMargins))
          .Build());
  SetAccessibleName(emoticon_label_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, kPickerEmoticonItemCornerRadius);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

PickerEmoticonItemView::~PickerEmoticonItemView() = default;

BEGIN_METADATA(PickerEmoticonItemView)
END_METADATA

}  // namespace ash
