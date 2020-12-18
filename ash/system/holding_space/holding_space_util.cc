// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace holding_space_util {

std::unique_ptr<views::Label> CreateLabel(LabelStyle style,
                                          const base::string16& text) {
  auto label = std::make_unique<views::Label>(text);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  switch (style) {
    case LabelStyle::kBadge:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case LabelStyle::kBody:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kChip:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kHeader:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM));
      break;
  }

  return label;
}

}  // namespace holding_space_util
}  // namespace ash
