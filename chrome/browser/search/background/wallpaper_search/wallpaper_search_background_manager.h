// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager_observer.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"
#include "components/prefs/pref_change_registrar.h"
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
  virtual std::vector<HistoryEntry> GetHistory();

  // Returns whether the passed in token matches the current wallpaper
  // search background.
  virtual bool IsCurrentBackground(const base::Token& id);

  // Sets a history image to the NTP background and sets matching theme color.
  virtual void SelectHistoryImage(const base::Token& id,
                                  const gfx::Image& image,
                                  base::ElapsedTimer timer);

  // Invoked by Wallpaper Search to set background image with already decoded
  // data.
  virtual void SelectLocalBackgroundImage(const base::Token& id,
                                          const SkBitmap& bitmap,
                                          bool is_inspiration_image,
                                          base::ElapsedTimer timer);

  // Saves the background to history if it is the current background.
  // Returns the backround's ID if successful.
  virtual std::optional<base::Token> SaveCurrentBackgroundToHistory(
      const HistoryEntry& history_entry);

  // Adds/Removes WallpaperSearchBackgroundManagerObserver observers.
  virtual void AddObserver(WallpaperSearchBackgroundManagerObserver* observer);
  void RemoveObserver(WallpaperSearchBackgroundManagerObserver* observer);

 private:
  void SetBackgroundToLocalResourceWithId(const base::Token& id,
                                          base::ElapsedTimer timer,
                                          const SkBitmap& bitmap,
                                          bool is_inspiration_image);

  void NotifyAboutHistory();

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  base::ObserverList<WallpaperSearchBackgroundManagerObserver> observers_;

  base::WeakPtrFactory<WallpaperSearchBackgroundManager> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_H_
