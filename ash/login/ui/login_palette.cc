// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_palette.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

LoginPalette CreateDefaultLoginPalette() {
  auto* color_provider = AshColorProvider::Get();
  auto background_color = color_provider->GetBackgroundColor();
  const AshColorProvider::RippleAttributes ripple_attributes =
      color_provider->GetRippleAttributes(background_color);
  // Convert transparency level from [0 ; 1] to [0 ; 255].
  U8CPU inkdrop_opacity = 255 * ripple_attributes.inkdrop_opacity;
  U8CPU highlight_opacity = 255 * ripple_attributes.highlight_opacity;
  return LoginPalette(
      {.password_text_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorPrimary),
       .password_placeholder_text_color =
           login_constants::kAuthMethodsTextColor,
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kIconColorPrimary),
       .button_annotation_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorSecondary),
       .pin_ink_drop_highlight_color =
           SkColorSetA(ripple_attributes.base_color, highlight_opacity),
       .pin_ink_drop_ripple_color =
           SkColorSetA(ripple_attributes.base_color, inkdrop_opacity)});
}

LoginPalette CreateInSessionAuthPalette() {
  return LoginPalette(
      {.password_text_color = SK_ColorDKGRAY,
       .password_placeholder_text_color = SK_ColorDKGRAY,
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = SK_ColorDKGRAY,
       .button_annotation_color = SK_ColorDKGRAY,
       .pin_ink_drop_highlight_color = SkColorSetA(SK_ColorDKGRAY, 0x0A),
       .pin_ink_drop_ripple_color = SkColorSetA(SK_ColorDKGRAY, 0x0F)});
}

}  // namespace ash
