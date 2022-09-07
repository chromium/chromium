// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_button.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "base/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CaptureModeButton::CaptureModeButton(views::Button::PressedCallback callback,
                                     const gfx::VectorIcon& icon)
    : views::ImageButton(callback) {
  ConfigureButton(this, views::FocusRing::Get(this));
  const SkColor normal_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, normal_color));
}

// static
void CaptureModeButton::ConfigureButton(views::ImageButton* button,
                                        views::FocusRing* focus_ring) {
  StyleUtil::SetUpInkDropForButton(button, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  button->SetImageHorizontalAlignment(ALIGN_CENTER);
  button->SetImageVerticalAlignment(ALIGN_MIDDLE);
  button->SetPreferredSize(capture_mode::kButtonSize);
  button->SetBorder(views::CreateEmptyBorder(capture_mode::kButtonPadding));
  button->GetViewAccessibility().OverrideIsLeaf(true);

  button->SetInstallFocusRingOnFocus(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          capture_mode::kButtonPadding));
  views::InstallCircleHighlightPathGenerator(button,
                                             capture_mode::kButtonPadding);
}

views::View* CaptureModeButton::GetView() {
  return this;
}

BEGIN_METADATA(CaptureModeButton, views::ImageButton)
END_METADATA

}  // namespace ash
