// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_button.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CaptureModeButton::CaptureModeButton(views::Button::PressedCallback callback,
                                     const gfx::VectorIcon& icon)
    : views::ImageButton(callback) {
  ConfigureButton(this, focus_ring());
  const SkColor normal_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, normal_color));

  // TODO(afakhry): Fix this.
  GetViewAccessibility().OverrideName(GetClassName());
}

// static
void CaptureModeButton::ConfigureButton(views::ImageButton* button,
                                        views::FocusRing* focus_ring) {
  button->ink_drop()->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  button->ink_drop()->SetVisibleOpacity(capture_mode::kInkDropVisibleOpacity);
  views::InkDrop::UseInkDropForFloodFillRipple(button->ink_drop(),
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/false);
  button->ink_drop()->SetCreateHighlightCallback(base::BindRepeating(
      [](views::Button* host) {
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), host->ink_drop()->GetBaseColor());
        highlight->set_visible_opacity(
            capture_mode::kInkDropHighlightVisibleOpacity);
        return highlight;
      },
      button));
  button->ink_drop()->SetBaseColor(capture_mode::kInkDropBaseColor);

  button->SetImageHorizontalAlignment(ALIGN_CENTER);
  button->SetImageVerticalAlignment(ALIGN_MIDDLE);
  button->SetPreferredSize(capture_mode::kButtonSize);
  button->SetBorder(views::CreateEmptyBorder(capture_mode::kButtonPadding));
  button->GetViewAccessibility().OverrideIsLeaf(true);

  button->SetInstallFocusRingOnFocus(true);
  focus_ring->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
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
