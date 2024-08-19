// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_utils.h"

#include <optional>

#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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
