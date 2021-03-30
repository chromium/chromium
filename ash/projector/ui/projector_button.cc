// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_button.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr gfx::Insets kButtonPadding{0};

}  // namespace

ProjectorButton::ProjectorButton(views::Button::PressedCallback callback)
    : ToggleImageButton(callback) {
  SetPreferredSize({kProjectorButtonSize, kProjectorButtonSize});
  SetBorder(views::CreateEmptyBorder(kButtonPadding));

  // Rounded background.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kProjectorButtonSize / 2.f);
}

std::unique_ptr<views::InkDrop> ProjectorButton::CreateInkDrop() {
  std::unique_ptr<views::InkDrop> ink_drop = views::Button::CreateInkDrop();
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(true);
  return ink_drop;
}

void ProjectorButton::OnPaintBackground(gfx::Canvas* canvas) {
  auto* color_provider = AshColorProvider::Get();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color_provider->GetControlsLayerColor(
      GetToggled()
          ? AshColorProvider::ControlsLayerType::kControlBackgroundColorActive
          : AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive));
  const gfx::RectF bounds(GetContentsBounds());
  canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, flags);
}

void ProjectorButton::OnThemeChanged() {
  views::ToggleImageButton::OnThemeChanged();

  // Ink Drop.
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetInkDropBaseColor(ripple_attributes.base_color);
  SetInkDropHighlightOpacity(ripple_attributes.highlight_opacity);
}

}  // namespace ash
