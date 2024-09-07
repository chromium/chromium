// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_

#include <optional>

#include "chrome/browser/themes/theme_service.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefService;

// Gets the pref service grayscale theme preference.
bool CurrentThemeIsGrayscale(const PrefService* pref_service);

// Gets the pref service user color preference.
std::optional<SkColor> CurrentThemeUserColor(const PrefService* pref_service);

sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant
BrowserColorVariantToProtoEnum(ui::mojom::BrowserColorVariant color_variant);

ui::mojom::BrowserColorVariant ProtoEnumToBrowserColorVariant(
    sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant color_variant);

sync_pb::ThemeSpecifics::BrowserColorScheme BrowserColorSchemeToProtoEnum(
    ThemeService::BrowserColorScheme color_scheme);

ThemeService::BrowserColorScheme ProtoEnumToBrowserColorScheme(
    sync_pb::ThemeSpecifics::BrowserColorScheme color_scheme);

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_UTILS_H_
