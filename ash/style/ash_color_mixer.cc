// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace ash {

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  if (!features::IsDarkLightModeEnabled())
    return;

  const SkColor menu_background_color =
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80);
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[ui::kColorAshSystemUIMenuBackground] = {menu_background_color};
}

}  // namespace ash
