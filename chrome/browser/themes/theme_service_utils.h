// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_

#include <optional>

#include "chrome/browser/themes/theme_service.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_provider_key.h"

class PrefService;

// Four ARGB colors used by Android Java custom theme customization
// (`NtpThemeColorFromHexInfo` and `NtpBackgroundDataCustomizedColor`; the 9
// built-in `@NtpThemeColorId` presets use static XML resources instead) to
// apply a dynamic color theme to the Activity and tint the NTP surfaces.
struct AndroidCustomThemeColors {
  SkColor primary_light = SK_ColorTRANSPARENT;
  SkColor primary_dark = SK_ColorTRANSPARENT;
  SkColor background_light = SK_ColorTRANSPARENT;
  SkColor background_dark = SK_ColorTRANSPARENT;

  bool operator==(const AndroidCustomThemeColors&) const = default;
};

// Gets the pref service grayscale theme preference.
bool CurrentThemeIsGrayscale(const PrefService* pref_service);

// Gets the pref service user color preference.
std::optional<SkColor> CurrentThemeUserColor(const PrefService* pref_service);

sync_pb::UserColorTheme::BrowserColorVariant BrowserColorVariantToProtoEnum(
    ui::mojom::BrowserColorVariant color_variant);

ui::mojom::BrowserColorVariant ProtoEnumToBrowserColorVariant(
    sync_pb::UserColorTheme::BrowserColorVariant color_variant);

// Returns `std::nullopt` for `BrowserColorVariant::kSystem`.
std::optional<ui::ColorProviderKey::SchemeVariant>
BrowserColorVariantToSchemeVariant(
    ui::mojom::BrowserColorVariant color_variant);

sync_pb::ThemeSpecifics::BrowserColorScheme BrowserColorSchemeToProtoEnum(
    ThemeService::BrowserColorScheme color_scheme);

ThemeService::BrowserColorScheme ProtoEnumToBrowserColorScheme(
    sync_pb::ThemeSpecifics::BrowserColorScheme color_scheme);

// Generates the light and dark primary and background colors for Android Java
// custom theme customization (`NtpThemeColorFromHexInfo` /
// `NtpBackgroundDataCustomizedColor`) from `seed_color` and `variant`
// (`kSystem` defaults to `kTonalSpot`).
AndroidCustomThemeColors GenerateAndroidCustomThemeColors(
    SkColor seed_color,
    ui::mojom::BrowserColorVariant variant);

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
