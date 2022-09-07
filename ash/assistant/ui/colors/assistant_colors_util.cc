// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/colors/assistant_colors_util.h"

#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/feature_list.h"

namespace ash {
namespace assistant {
namespace {

SkColor GetDarkLightModeFlagOffColor(assistant_colors::ColorName color_name) {
  if (color_name == assistant_colors::ColorName::kBgAssistantPlate)
    return SK_ColorWHITE;

  return assistant_colors::ResolveColor(color_name, /*is_dark_mode=*/false,
                                        /*use_debug_colors=*/false);
}

}  // namespace

SkColor ResolveAssistantColor(assistant_colors::ColorName color_name) {
  // Delete this utility class and call assistant_colors::ResolveColor directly
  // once dark and light mode has launched and features::IsDarkLightModeEnabled
  // gets removed.
  if (!UseDarkLightModeColors())
    return GetDarkLightModeFlagOffColor(color_name);

  return assistant_colors::ResolveColor(
      color_name, DarkLightModeController::Get()->IsDarkModeEnabled(),
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

bool UseDarkLightModeColors() {
  return features::IsDarkLightModeEnabled() ||
         features::IsProductivityLauncherEnabled();
}

}  // namespace assistant
}  // namespace ash
