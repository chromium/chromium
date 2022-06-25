// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace ash {

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
  mixer[ui::kColorAshFocusRing] = {ash_color_provider->GetControlsLayerColor(
      ash::AshColorProvider::ControlsLayerType::kFocusRingColor)};
  mixer[ui::kColorAshEditFinishFocusRing] = {gfx::kGoogleBlue300};
  mixer[ui::kColorAshIconInOobe] = {kIconColorInOobe};

  // TODO(skau): Remove when dark/light mode launches.
  mixer[ui::kColorAshAppListFocusRingCompat] = {gfx::kGoogleBlue600};

  mixer[ui::kColorAshLightFocusRing] = {gfx::kGoogleBlue300};

  mixer[ui::kColorAshOnboardingFocusRing] = {gfx::kGoogleBlue300};

  mixer[ui::kColorAshSystemUIBorderColor1] = {
      ash_color_provider->GetControlsLayerColor(
          ash::AshColorProvider::ControlsLayerType::kBorderColor1)};
  mixer[ui::kColorAshSystemUIBorderColor2] = {
      ash_color_provider->GetControlsLayerColor(
          ash::AshColorProvider::ControlsLayerType::kBorderColor2)};
  mixer[ui::kColorAshSystemUIHighlightColor1] = {
      ash_color_provider->GetControlsLayerColor(
          ash::AshColorProvider::ControlsLayerType::kHighlightColor1)};
  mixer[ui::kColorAshSystemUIHighlightColor2] = {
      ash_color_provider->GetControlsLayerColor(
          ash::AshColorProvider::ControlsLayerType::kHighlightColor2)};

  if (!features::IsDarkLightModeEnabled()) {
    ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
    mixer[ui::kColorAshSystemUILightBorderColor1] = {
        ash_color_provider->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kBorderColor1)};
    mixer[ui::kColorAshSystemUILightBorderColor2] = {
        ash_color_provider->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kBorderColor1)};
    mixer[ui::kColorAshSystemUILightHighlightColor1] = {
        ash_color_provider->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kHighlightColor1)};
    mixer[ui::kColorAshSystemUILightHighlightColor2] = {
        ash_color_provider->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kHighlightColor2)};
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
