// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/save_desk_template_button.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/focus_ring.h"
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
  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<SaveDeskTemplateButton*>(view)->IsViewHighlighted();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        /*corner_radius=*/kCornerRadius,
        views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false));
  }
}

SaveDeskTemplateButton::~SaveDeskTemplateButton() = default;

views::View* SaveDeskTemplateButton::GetView() {
  return this;
}

void SaveDeskTemplateButton::MaybeActivateHighlightedView() {
  if (GetEnabled())
    callback_.Run();
}

void SaveDeskTemplateButton::MaybeCloseHighlightedView(bool primary_action) {}

void SaveDeskTemplateButton::MaybeSwapHighlightedView(bool right) {}

void SaveDeskTemplateButton::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SaveDeskTemplateButton::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SaveDeskTemplateButton::OnThemeChanged() {
  PillButton::OnThemeChanged();
  SetBackgroundColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
}

BEGIN_METADATA(SaveDeskTemplateButton, PillButton)
END_METADATA

}  // namespace ash
