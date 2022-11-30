// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_palette.h"

#include "ash/style/ash_color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

LoginPalette CreateDefaultLoginPalette(ui::ColorProvider* color_provider) {
  // `color_provider` should be initialized as `view::GetColorProvider()`, which
  // might be nullptr before the view isadded to the widget. Make sure to
  // override the colors inside `OnThemeChanged` which will be called after the
  // view is added to the widget hierarchy.
  if (!color_provider)
    return LoginPalette();

  return LoginPalette(
      {.password_text_color =
           color_provider->GetColor(kColorAshTextColorPrimary),
       .password_placeholder_text_color =
           color_provider->GetColor(kColorAshTextColorSecondary),
       .password_background_color = SK_ColorTRANSPARENT,
       .password_row_background_color =
           color_provider->GetColor(kColorAshControlBackgroundColorInactive),
       .button_enabled_color =
           color_provider->GetColor(kColorAshIconColorPrimary),
       .button_annotation_color =
           color_provider->GetColor(kColorAshTextColorSecondary),
       .pin_ink_drop_highlight_color =
           color_provider->GetColor(kColorAshInkDrop),
       .pin_ink_drop_ripple_color = color_provider->GetColor(kColorAshInkDrop),
       .pin_input_text_color =
           color_provider->GetColor(kColorAshTextColorPrimary),
       .submit_button_background_color =
           color_provider->GetColor(kColorAshControlBackgroundColorInactive),
       .submit_button_icon_color =
           color_provider->GetColor(kColorAshButtonIconColor)});
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
