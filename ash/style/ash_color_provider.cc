// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_palette_controller.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_utils.h"

namespace ash {

using ColorName = cros_styles::ColorName;

namespace {

// Opacity of the light/dark indrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.06f;

AshColorProvider* g_instance = nullptr;

}  // namespace

AshColorProvider::AshColorProvider() {
  DCHECK(!g_instance);
  g_instance = this;
}

AshColorProvider::~AshColorProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AshColorProvider* AshColorProvider::Get() {
  return g_instance;
}

SkColor AshColorProvider::GetColor(ui::ColorId color_id) const {
  auto* color_provider = GetColorProvider();
  return color_provider->GetColor(color_id);
}

std::pair<SkColor, float> AshColorProvider::GetInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_dark = color_utils::IsDark(background_color);
  const SkColor base_color =
      GetColorProvider()->GetColor(kColorAshInkDropOpaqueColor);
  const float opacity = is_dark ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

SkColor AshColorProvider::GetBackgroundColor() const {
  const auto default_color =
      GetColorProvider()->GetColor(kColorAshShieldAndBaseOpaque);
  if (!Shell::HasInstance()) {
    CHECK_IS_TEST();
    return default_color;
  }
  return Shell::Get()
      ->color_palette_controller()
      ->GetUserWallpaperColorOrDefault(default_color);
}

ui::ColorProvider* AshColorProvider::GetColorProvider() const {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
}

}  // namespace ash
