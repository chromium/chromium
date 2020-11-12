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
       .password_placeholder_text_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorSecondary),
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kIconColorPrimary),
       .button_annotation_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorSecondary),
       .pin_ink_drop_highlight_color =
           SkColorSetA(ripple_attributes.base_color, highlight_opacity),
       .pin_ink_drop_ripple_color =
           SkColorSetA(ripple_attributes.base_color, inkdrop_opacity),
       .pin_input_text_color = AshColorProvider::Get()->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorPrimary)});
}

LoginPalette CreateInSessionAuthPalette() {
  return LoginPalette(
      {.password_text_color = gfx::kGoogleGrey900,
       .password_placeholder_text_color = gfx::kGoogleGrey900,
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = gfx::kGoogleGrey900,
       .button_annotation_color = gfx::kGoogleGrey700,
       .pin_ink_drop_highlight_color = SkColorSetA(gfx::kGoogleGrey900, 0x0A),
       .pin_ink_drop_ripple_color = SkColorSetA(gfx::kGoogleGrey900, 0x0F),
       .pin_input_text_color = gfx::kGoogleGrey900});
}

}  // namespace ash
