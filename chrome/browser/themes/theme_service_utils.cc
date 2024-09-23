// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_utils.h"

#include <optional>

#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "third_party/skia/include/core/SkColor.h"

bool CurrentThemeIsGrayscale(const PrefService* pref_service) {
  return pref_service->GetBoolean(GetThemePrefNameInMigration(
      ThemePrefInMigration::kGrayscaleThemeEnabled));
}

std::optional<SkColor> CurrentThemeUserColor(const PrefService* pref_service) {
  const SkColor user_color = pref_service->GetInteger(
      GetThemePrefNameInMigration(ThemePrefInMigration::kUserColor));
  return user_color == SK_ColorTRANSPARENT ? std::nullopt
                                           : std::make_optional(user_color);
}

sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant
BrowserColorVariantToProtoEnum(ui::mojom::BrowserColorVariant color_variant) {
  switch (color_variant) {
    case ui::mojom::BrowserColorVariant::kSystem:
      return sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_SYSTEM;
    case ui::mojom::BrowserColorVariant::kTonalSpot:
      return sync_pb::
          ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT;
    case ui::mojom::BrowserColorVariant::kNeutral:
      return sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_NEUTRAL;
    case ui::mojom::BrowserColorVariant::kVibrant:
      return sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_VIBRANT;
    case ui::mojom::BrowserColorVariant::kExpressive:
      return sync_pb::
          ThemeSpecifics_UserColorTheme_BrowserColorVariant_EXPRESSIVE;
  }
}

ui::mojom::BrowserColorVariant ProtoEnumToBrowserColorVariant(
    sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant
        color_variant) {
  switch (color_variant) {
    case sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_SYSTEM:
      return ui::mojom::BrowserColorVariant::kSystem;
    case sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT:
      return ui::mojom::BrowserColorVariant::kTonalSpot;
    case sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_NEUTRAL:
      return ui::mojom::BrowserColorVariant::kNeutral;
    case sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_VIBRANT:
      return ui::mojom::BrowserColorVariant::kVibrant;
    case sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_EXPRESSIVE:
      return ui::mojom::BrowserColorVariant::kExpressive;
    case sync_pb::
        ThemeSpecifics_UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED:
      return ui::mojom::BrowserColorVariant::kSystem;
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
