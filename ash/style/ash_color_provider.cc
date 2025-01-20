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

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  // TODO(crbug.com/1292244): Delete this function after all callers migrate.
  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  switch (type) {
    case ControlsLayerType::kControlBackgroundColorActive:
      return color_provider->GetColor(kColorAshControlBackgroundColorActive);
    case ControlsLayerType::kControlBackgroundColorInactive:
      return color_provider->GetColor(kColorAshControlBackgroundColorInactive);
    case ControlsLayerType::kFocusRingColor:
      return color_provider->GetColor(ui::kColorAshFocusRing);
  }
}

SkColor AshColorProvider::GetContentLayerColor(ContentLayerType type) const {
  auto* color_provider = GetColorProvider();
  switch (type) {
    case ContentLayerType::kSeparatorColor:
      return color_provider->GetColor(kColorAshSeparatorColor);
    case ContentLayerType::kIconColorSecondary:
      return color_provider->GetColor(kColorAshIconColorSecondary);
    case ContentLayerType::kIconColorSecondaryBackground:
      return color_provider->GetColor(kColorAshIconColorSecondaryBackground);
    case ContentLayerType::kSwitchKnobColorInactive:
      return color_provider->GetColor(kColorAshSwitchKnobColorInactive);
    case ContentLayerType::kSwitchTrackColorInactive:
      return color_provider->GetColor(kColorAshSwitchTrackColorInactive);
    case ContentLayerType::kButtonLabelColorBlue:
      return color_provider->GetColor(kColorAshButtonLabelColorBlue);
    case ContentLayerType::kTextColorURL:
      return color_provider->GetColor(kColorAshTextColorURL);
    case ContentLayerType::kSwitchKnobColorActive:
      return color_provider->GetColor(kColorAshSwitchKnobColorActive);
    case ContentLayerType::kSwitchTrackColorActive:
      return color_provider->GetColor(kColorAshSwitchTrackColorActive);
    case ContentLayerType::kButtonIconColorPrimary:
      return color_provider->GetColor(kColorAshButtonIconColorPrimary);
    case ContentLayerType::kButtonIconColor:
      return color_provider->GetColor(kColorAshButtonIconColor);
    case ContentLayerType::kButtonLabelColor:
      return color_provider->GetColor(kColorAshButtonLabelColor);
    case ContentLayerType::kTextColorPrimary:
      return color_provider->GetColor(kColorAshTextColorPrimary);
    case ContentLayerType::kTextColorSecondary:
      return color_provider->GetColor(kColorAshTextColorSecondary);
    case ContentLayerType::kIconColorPrimary:
      return color_provider->GetColor(kColorAshIconColorPrimary);
    case ContentLayerType::kIconColorAlert:
      return color_provider->GetColor(kColorAshIconColorAlert);
    case ContentLayerType::kIconColorWarning:
      return color_provider->GetColor(kColorAshIconColorWarning);
    case ContentLayerType::kIconColorProminent:
      return color_provider->GetColor(kColorAshIconColorProminent);
  }
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
