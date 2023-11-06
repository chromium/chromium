// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

// Manages wallpaper search backgrounds in the customize chrome side panel.
class WallpaperSearchBackgroundManager {
 public:
  explicit WallpaperSearchBackgroundManager(Profile* profile);
  virtual ~WallpaperSearchBackgroundManager();

  // Invoked by Wallpaper Search to set background image with already decoded
  // data.
  virtual void SelectLocalBackgroundImage(const base::Token& id,
                                          const SkBitmap& bitmap);

 private:
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
