// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace ash {

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  auto* ash_color_provider = AshColorProvider::Get();
  ui::ColorMixer& mixer = provider->AddMixer();

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
