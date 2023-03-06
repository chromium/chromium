// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_PREFS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_PREFS_H_

class PrefRegistrySimple;

namespace wallpaper_handlers::prefs {

// Boolean that specifies whether or not wallpaper images may be selected from
// a user's Google Photos albums.
extern const char kWallpaperGooglePhotosIntegrationEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace wallpaper_handlers::prefs

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_PREFS_H_
