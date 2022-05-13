// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/save_desk_template_button.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/highlight_border.h"

namespace ash {

constexpr int kCornerRadius = 16;

SaveDeskTemplateButton::SaveDeskTemplateButton(base::RepeatingClosure callback,
                                               const std::u16string& text,
                                               Type button_type,
                                               const gfx::VectorIcon* icon)
    : PillButton(callback, text, PillButton::Type::kIcon, icon),
      callback_(callback),
      button_type_(button_type) {
  SetBorder(std::make_unique<WmHighlightItemBorder>(kCornerRadius));
}

SaveDeskTemplateButton::~SaveDeskTemplateButton() = default;

views::View* SaveDeskTemplateButton::GetView() {
  return this;
}

void SaveDeskTemplateButton::MaybeActivateHighlightedView() {
  callback_.Run();
}

void SaveDeskTemplateButton::MaybeCloseHighlightedView(bool primary_action) {}

void SaveDeskTemplateButton::MaybeSwapHighlightedView(bool right) {}

void SaveDeskTemplateButton::OnViewHighlighted() {
  UpdateBorderState();
}

void SaveDeskTemplateButton::OnViewUnhighlighted() {
  UpdateBorderState();
}

void SaveDeskTemplateButton::OnThemeChanged() {
  PillButton::OnThemeChanged();
  SetBackgroundColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
  UpdateBorderState();
}

void SaveDeskTemplateButton::OnPaintBorder(gfx::Canvas* canvas) {
  if (features::IsDarkLightModeEnabled()) {
    views::HighlightBorder::PaintBorderToCanvas(
        canvas, *this, GetLocalBounds(), gfx::RoundedCornersF(kCornerRadius),
        views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false);
  }
}

void SaveDeskTemplateButton::UpdateBorderState() {
  auto* border = static_cast<WmHighlightItemBorder*>(GetBorder());
  border->SetFocused(IsViewHighlighted());
  SchedulePaint();
}

BEGIN_METADATA(SaveDeskTemplateButton, PillButton)
END_METADATA

}  // namespace ash
