// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_image_button.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

ProjectorImageButton::ProjectorImageButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& name)
    : ProjectorButton(callback, name) {
  SetVectorIcon(icon);
}

void ProjectorImageButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, normal_color));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(
               icon, color_provider->GetDisabledColor(normal_color)));
  const SkColor toggled_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColorPrimary);
  const auto toggled_icon = gfx::CreateVectorIcon(icon, toggled_color);
  SetToggledImage(views::Button::STATE_NORMAL, &toggled_icon);
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
}

}  // namespace ash
