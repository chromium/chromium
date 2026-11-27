// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_utils.h"

#include <memory>
#include <optional>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"

bool CurrentThemeIsGrayscale(const PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kGrayscaleThemeEnabled);
}

std::optional<SkColor> CurrentThemeUserColor(const PrefService* pref_service) {
  const SkColor user_color = pref_service->GetInteger(prefs::kUserColor);
  return user_color == SK_ColorTRANSPARENT ? std::nullopt
                                           : std::make_optional(user_color);
}

sync_pb::UserColorTheme::BrowserColorVariant BrowserColorVariantToProtoEnum(
    ui::mojom::BrowserColorVariant color_variant) {
  switch (color_variant) {
    case ui::mojom::BrowserColorVariant::kSystem:
      return sync_pb::UserColorTheme_BrowserColorVariant_SYSTEM;
    case ui::mojom::BrowserColorVariant::kTonalSpot:
      return sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT;
    case ui::mojom::BrowserColorVariant::kNeutral:
      return sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL;
    case ui::mojom::BrowserColorVariant::kVibrant:
      return sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT;
    case ui::mojom::BrowserColorVariant::kExpressive:
      return sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE;
  }
}

ui::mojom::BrowserColorVariant ProtoEnumToBrowserColorVariant(
    sync_pb::UserColorTheme::BrowserColorVariant color_variant) {
  switch (color_variant) {
    case sync_pb::UserColorTheme_BrowserColorVariant_SYSTEM:
      return ui::mojom::BrowserColorVariant::kSystem;
    case sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT:
      return ui::mojom::BrowserColorVariant::kTonalSpot;
    case sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL:
      return ui::mojom::BrowserColorVariant::kNeutral;
    case sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT:
      return ui::mojom::BrowserColorVariant::kVibrant;
    case sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE:
      return ui::mojom::BrowserColorVariant::kExpressive;
    case sync_pb::
        UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED:
      return ui::mojom::BrowserColorVariant::kSystem;
  }
}

std::optional<ui::ColorProviderKey::SchemeVariant>
BrowserColorVariantToSchemeVariant(
    ui::mojom::BrowserColorVariant color_variant) {
  switch (color_variant) {
    case ui::mojom::BrowserColorVariant::kSystem:
      return std::nullopt;
    case ui::mojom::BrowserColorVariant::kTonalSpot:
      return ui::ColorProviderKey::SchemeVariant::kTonalSpot;
    case ui::mojom::BrowserColorVariant::kNeutral:
      return ui::ColorProviderKey::SchemeVariant::kNeutral;
    case ui::mojom::BrowserColorVariant::kVibrant:
      return ui::ColorProviderKey::SchemeVariant::kVibrant;
    case ui::mojom::BrowserColorVariant::kExpressive:
      return ui::ColorProviderKey::SchemeVariant::kExpressive;
  }
}

sync_pb::ThemeSpecifics::BrowserColorScheme BrowserColorSchemeToProtoEnum(
    ThemeService::BrowserColorScheme color_scheme) {
  switch (color_scheme) {
    case ThemeService::BrowserColorScheme::kSystem:
      return sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM;
    case ThemeService::BrowserColorScheme::kLight:
      return sync_pb::ThemeSpecifics_BrowserColorScheme_LIGHT;
    case ThemeService::BrowserColorScheme::kDark:
      return sync_pb::ThemeSpecifics_BrowserColorScheme_DARK;
  }
}

ThemeService::BrowserColorScheme ProtoEnumToBrowserColorScheme(
    sync_pb::ThemeSpecifics::BrowserColorScheme color_scheme) {
  switch (color_scheme) {
    case sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM:
      return ThemeService::BrowserColorScheme::kSystem;
    case sync_pb::ThemeSpecifics_BrowserColorScheme_LIGHT:
      return ThemeService::BrowserColorScheme::kLight;
    case sync_pb::ThemeSpecifics_BrowserColorScheme_DARK:
      return ThemeService::BrowserColorScheme::kDark;
    case sync_pb::
        ThemeSpecifics_BrowserColorScheme_BROWSER_COLOR_SCHEME_UNSPECIFIED:
      return ThemeService::BrowserColorScheme::kSystem;
  }
}

AndroidCustomThemeColors GenerateAndroidCustomThemeColors(
    SkColor seed_color,
    ui::mojom::BrowserColorVariant variant) {
  std::unique_ptr<ui::Palette> palette = ui::GeneratePalette(
      seed_color, BrowserColorVariantToSchemeVariant(variant).value_or(
                      ui::ColorProviderKey::SchemeVariant::kTonalSpot));
  // Why `primary_dark` uses Tone 67 instead of Tone 80:
  // Java's `DynamicColorsOptions.Builder.setContentBasedSource()` constructs a
  // `SchemeContent` (Fidelity) palette, which pins `colorPrimaryContainer` to
  // the input seed's tone and enforces a `ToneDeltaPair` constraint keeping
  // `colorPrimary` at least 10 tone units further from the surface. Passing
  // Tone 67 (`palette->primary().get(67)`) satisfies `80 - 67 = 13 >= 10`, so
  // Java's dark-mode `colorPrimary` lands at Tone 80 (matching C++
  // `kColorSysPrimary` and the 9 presets in `one_off_colors.xml`), whereas
  // passing Tone 80 pushes Java's dark-mode `colorPrimary` up to Tone 90
  // (washed out).
  return AndroidCustomThemeColors{
      .primary_light = palette->primary().get(40),
      .primary_dark = palette->primary().get(67),
      .background_light = palette->neutral().get(98),
      .background_dark = palette->secondary().get(25),
  };
}
