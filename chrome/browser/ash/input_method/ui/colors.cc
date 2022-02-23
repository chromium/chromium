// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/colors.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"

namespace ui {
namespace ime {

bool IsDarkModeEnabled() {
  auto* provider = ash::ColorProvider::Get();
  if (!provider)
    return false;
  return (ash::features::IsDarkLightModeEnabled() &&
          provider->IsDarkModeEnabled());
}

SkColor ResolveSemanticColor(const cros_styles::ColorName& color_name) {
  return cros_styles::ResolveColor(color_name, IsDarkModeEnabled(), false);
}

}  // namespace ime
}  // namespace ui
