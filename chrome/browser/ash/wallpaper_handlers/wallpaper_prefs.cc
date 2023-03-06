// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace wallpaper_handlers::prefs {

const char kWallpaperGooglePhotosIntegrationEnabled[] =
    "wallpaper_google_photos_integration_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWallpaperGooglePhotosIntegrationEnabled, true);
}

}  // namespace wallpaper_handlers::prefs
