// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_close_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

CaptureModeCloseButton::CaptureModeCloseButton(views::ButtonListener* listener)
    : ViewWithInkDrop(listener) {
  SetPreferredSize(capture_mode::kButtonSize);
  SetBorder(views::CreateEmptyBorder(capture_mode::kButtonPadding));
  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kCloseButtonIcon, normal_color));
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  GetViewAccessibility().OverrideIsLeaf(true);

  // TODO(afakhry): Fix this.
  GetViewAccessibility().OverrideName(GetClassName());

  SetInstallFocusRingOnFocus(true);
  SetFocusForPlatform();
  focus_ring()->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  focus_ring()->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          capture_mode::kButtonPadding));
  views::InstallCircleHighlightPathGenerator(this,
                                             capture_mode::kButtonPadding);
}

const char* CaptureModeCloseButton::GetClassName() const {
  return "CaptureModeCloseButton";
}

}  // namespace ash
