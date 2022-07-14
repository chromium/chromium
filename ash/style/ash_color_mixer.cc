// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace ash {

namespace {

constexpr int kAlpha20 = SK_AlphaOPAQUE * 0.2f;
constexpr int kAlpha40 = SK_AlphaOPAQUE * 0.4f;
constexpr int kAlpha60 = SK_AlphaOPAQUE * 0.6f;
constexpr int kAlpha80 = SK_AlphaOPAQUE * 0.8f;
constexpr int kAlpha90 = SK_AlphaOPAQUE * 0.9f;
constexpr int kAlpha95 = SK_AlphaOPAQUE * 0.95f;

void AddShieldAndBaseColors(ui::ColorMixer& mixer,
                            const ui::ColorProviderManager::Key& key) {
  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  // Colors of the Shield and Base layers.
  const SkColor default_background_color =
      use_dark_color ? gfx::kGoogleGrey900 : SK_ColorWHITE;
  // TODO(minch|skau): Investigate/fix whether should DCHECK the existence of
  // the value of `use_color` here.
  const SkColor background_color =
      key.user_color.value_or(default_background_color);
  mixer[kColorAshShieldAndBase20] = {SkColorSetA(background_color, kAlpha20)};
  mixer[kColorAshShieldAndBase40] = {SkColorSetA(background_color, kAlpha40)};
  mixer[kColorAshShieldAndBase60] = {SkColorSetA(background_color, kAlpha60)};
  mixer[kColorAshShieldAndBase80] = {SkColorSetA(background_color, kAlpha80)};
  mixer[kColorAshShieldAndBase90] = {SkColorSetA(background_color, kAlpha90)};
  mixer[kColorAshShieldAndBase95] = {SkColorSetA(background_color, kAlpha95)};
  mixer[kColorAshShieldAndBaseOpaque] = {
      SkColorSetA(background_color, SK_AlphaOPAQUE)};
}

// Mappings of Controls Colors for Material 2.
void AddControlsColors(ui::ColorMixer& mixer,
                       const ui::ColorProviderManager::Key& key) {
  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  // ControlsLayer colors
  mixer[kColorAshHairlineBorderColor] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x24))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x24));
  mixer[kColorAshControlBackgroundColorActive] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleBlue300)
                     : ui::ColorTransform(gfx::kGoogleBlue600);
  mixer[kColorAshControlBackgroundColorInactive] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x1A))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x0D));
  mixer[kColorAshControlBackgroundColorAlert] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleRed300)
                     : ui::ColorTransform(gfx::kGoogleRed600);
  mixer[kColorAshControlBackgroundColorWarning] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleYellow300)
                     : ui::ColorTransform(gfx::kGoogleYellow600);
  mixer[kColorAshControlBackgroundColorPositive] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGreen300)
                     : ui::ColorTransform(gfx::kGoogleGreen600);
  mixer[kColorAshFocusAuraColor] =
      use_dark_color
          ? ui::ColorTransform(SkColorSetA(gfx::kGoogleBlue300, 0x3D))
          : ui::ColorTransform(SkColorSetA(gfx::kGoogleBlue600, 0x3D));
  mixer[ui::kColorAshFocusRing] = use_dark_color
                                      ? ui::ColorTransform(gfx::kGoogleBlue300)
                                      : ui::ColorTransform(gfx::kGoogleBlue600);

  mixer[ui::kColorAshSystemUIBorderColor1] =
      use_dark_color ? ui::ColorTransform(kColorAshShieldAndBase80)
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x0F));
  mixer[ui::kColorAshSystemUIBorderColor2] =
      use_dark_color ? ui::ColorTransform(kColorAshShieldAndBase60)
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x0F));
  mixer[ui::kColorAshSystemUIBorderColor3] = {SkColorSetA(SK_ColorBLACK, 0x0F)};

  mixer[ui::kColorAshSystemUIHighlightColor1] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x14))
                     : ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x4C));
  mixer[ui::kColorAshSystemUIHighlightColor2] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x0F))
                     : ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x33));
  mixer[ui::kColorAshSystemUIHighlightColor3] = {
      ui::kColorAshSystemUIHighlightColor1};
}

}  // namespace

void AddCrosStylesColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  bool dark_mode = key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  cros_tokens::AddCrosRefColorsToMixer(mixer, dark_mode);
  cros_tokens::AddCrosSysColorsToMixer(mixer, dark_mode);
  cros_tokens::AddLegacySemanticColorsToMixer(mixer, dark_mode);

  // TODO(b/235913438): Remap legacy colors to tokens here.
}

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  auto* ash_color_provider = AshColorProvider::Get();
  ui::ColorMixer& mixer = provider->AddMixer();

  AddShieldAndBaseColors(mixer, key);
  AddControlsColors(mixer, key);

  mixer[ui::kColorAshActionLabelFocusRingEdit] = {gfx::kGoogleBlue300};
  mixer[ui::kColorAshActionLabelFocusRingError] = {gfx::kGoogleRed300};
  mixer[ui::kColorAshActionLabelFocusRingHover] =
      ui::SetAlpha(gfx::kGoogleGrey200, 0x60);

  mixer[ui::kColorAshAppListFocusRingNoKeyboard] = {SK_AlphaTRANSPARENT};
  mixer[ui::kColorAshAppListSeparatorLight] = {
      ui::kColorAshSystemUIMenuSeparator};
  mixer[ui::kColorAshAppListSeparator] =
      ui::SetAlpha(gfx::kGoogleGrey900, 0x24);
  mixer[ui::kColorAshArcInputMenuSeparator] = {SK_ColorGRAY};
  mixer[ui::kColorAshEditFinishFocusRing] = {gfx::kGoogleBlue300};
  mixer[ui::kColorAshIconInOobe] = {kIconColorInOobe};

  // TODO(skau): Remove when dark/light mode launches.
  mixer[ui::kColorAshAppListFocusRingCompat] = {gfx::kGoogleBlue600};

  mixer[ui::kColorAshLightFocusRing] = {gfx::kGoogleBlue300};

  mixer[ui::kColorAshOnboardingFocusRing] = {gfx::kGoogleBlue300};

  if (!features::IsDarkLightModeEnabled()) {
    ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
    mixer[ui::kColorAshSystemUILightBorderColor1] = {
        ui::kColorAshSystemUIBorderColor1};
    mixer[ui::kColorAshSystemUILightBorderColor2] = {
        ui::kColorAshSystemUIBorderColor2};
    mixer[ui::kColorAshSystemUILightHighlightColor1] = {
        ui::kColorAshSystemUIHighlightColor1};
    mixer[ui::kColorAshSystemUILightHighlightColor2] = {
        ui::kColorAshSystemUIHighlightColor2};
    return;
  }

  mixer[ui::kColorAshSystemUIMenuBackground] = {
      ash_color_provider->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)};
  mixer[ui::kColorAshSystemUIMenuIcon] = {
      ash_color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)};

  auto [color, opacity] = ash_color_provider->GetInkDropBaseColorAndOpacity();
  mixer[ui::kColorAshSystemUIMenuItemBackgroundSelected] = {
      SkColorSetA(color, opacity * SK_AlphaOPAQUE)};
  mixer[ui::kColorAshSystemUIMenuSeparator] = {
      ash_color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kSeparatorColor)};
}

}  // namespace ash
