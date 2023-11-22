// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

class PrefRegistrySimple;
class Profile;

// Manages wallpaper search backgrounds in the customize chrome side panel.
class WallpaperSearchBackgroundManager {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RemoveWallpaperSearchBackground(Profile* profile);
  static void ResetProfilePrefs(Profile* profile);

  explicit WallpaperSearchBackgroundManager(Profile* profile);
  virtual ~WallpaperSearchBackgroundManager();

  // Gets the current history list.
  virtual std::vector<base::Token> GetHistory();

  // Sets a history image to the NTP background and sets matching theme color.
  virtual void SelectHistoryImage(const base::Token& id,
                                  const gfx::Image& image);

  // Invoked by Wallpaper Search to set background image with already decoded
  // data.
  virtual void SelectLocalBackgroundImage(const base::Token& id,
                                          const SkBitmap& bitmap);

  // Saves the current background to history if it is a wallpaper search one.
  // Returns the backround's ID if successful.
  virtual absl::optional<base::Token> SaveCurrentBackgroundToHistory();

 private:
  void SetBackgroundToLocalResourceWithId(const base::Token& id);

  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<WallpaperSearchBackgroundManager> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
