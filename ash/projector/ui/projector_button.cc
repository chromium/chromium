// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_button.h"

#include <utility>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr gfx::Insets kButtonPadding{0};

}  // namespace

ProjectorButton::ProjectorButton(views::Button::PressedCallback callback,
                                 const std::u16string& name)
    : ToggleImageButton(std::move(callback)) {
  SetPreferredSize(gfx::Size(kProjectorButtonSize, kProjectorButtonSize));
  SetBorder(views::CreateEmptyBorder(kButtonPadding));

  // Rounded background.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kProjectorButtonSize / 2.f);

  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/true,
                                               /*highlight_on_focus=*/true);

  SetTooltipText(name);

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(name);
}

void ProjectorButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (!GetToggled()) {
    return;
  }
  const auto* color_provider = GetColorProvider();
  // Draw a filled background for the button.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(
      color_provider->GetColor(kColorAshControlBackgroundColorInactive));
  const gfx::RectF bounds(GetContentsBounds());
  canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, flags);

  // Draw a border on the background circle.
  cc::PaintFlags border_flags;
  border_flags.setAntiAlias(true);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setColor(color_provider->GetColor(kColorAshHairlineBorderColor));
  border_flags.setStrokeWidth(kProjectorButtonBorderSize);
  canvas->DrawCircle(bounds.CenterPoint(),
                     (bounds.width() - kProjectorButtonBorderSize * 2) / 2,
                     border_flags);
}

void ProjectorButton::OnThemeChanged() {
  views::ToggleImageButton::OnThemeChanged();

  // Ink Drop.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  StyleUtil::ConfigureInkDropAttributes(
      this, StyleUtil::kBaseColor | StyleUtil::kHighlightOpacity);
}

BEGIN_METADATA(ProjectorButton)
END_METADATA

}  // namespace ash
