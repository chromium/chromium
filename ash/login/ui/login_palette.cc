// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_palette.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

LoginPalette CreateDefaultLoginPalette() {
  auto* color_provider = AshColorProvider::Get();
  const std::pair<SkColor, float> base_color_and_opacity =
      color_provider->GetInkDropBaseColorAndOpacity();
  // Convert transparency level from [0 ; 1] to [0 ; 255].
  U8CPU inkdrop_opacity = 255 * base_color_and_opacity.second;
  return LoginPalette(
      {.password_text_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorPrimary),
       .password_placeholder_text_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorSecondary),
       .password_background_color = SK_ColorTRANSPARENT,
       .password_row_background_color = color_provider->GetControlsLayerColor(
           AshColorProvider::ControlsLayerType::
               kControlBackgroundColorInactive),
       .button_enabled_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kIconColorPrimary),
       .button_annotation_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorSecondary),
       .pin_ink_drop_highlight_color =
           SkColorSetA(base_color_and_opacity.first, inkdrop_opacity),
       .pin_ink_drop_ripple_color =
           SkColorSetA(base_color_and_opacity.first, inkdrop_opacity),
       .pin_input_text_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kTextColorPrimary),
       .submit_button_background_color = color_provider->GetControlsLayerColor(
           AshColorProvider::ControlsLayerType::
               kControlBackgroundColorInactive),
       .submit_button_icon_color = color_provider->GetContentLayerColor(
           AshColorProvider::ContentLayerType::kButtonIconColor)});
}

// TODO(b/218610104): Support dark theme.
LoginPalette CreateInSessionAuthPalette() {
  return LoginPalette(
      {.password_text_color = gfx::kGoogleGrey900,
       .password_placeholder_text_color = gfx::kGoogleGrey600,
       .password_background_color = SK_ColorTRANSPARENT,
       .password_row_background_color = gfx::kGoogleGrey100,
       .button_enabled_color = gfx::kGoogleGrey900,
       .button_annotation_color = gfx::kGoogleGrey700,
       .pin_ink_drop_highlight_color = SkColorSetA(gfx::kGoogleGrey900, 0x0A),
       .pin_ink_drop_ripple_color = SkColorSetA(gfx::kGoogleGrey900, 0x0F),
       .pin_input_text_color = gfx::kGoogleGrey900,
       .submit_button_background_color = gfx::kGoogleGrey100,
       .submit_button_icon_color = gfx::kGoogleGrey900});
}

}  // namespace ash
