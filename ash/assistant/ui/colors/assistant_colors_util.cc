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

SkColor ResolveAssistantColor(assistant_colors::ColorName color_name) {
  return assistant_colors::ResolveColor(
      color_name, DarkLightModeController::Get()->IsDarkModeEnabled(),
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

}  // namespace assistant
}  // namespace ash
