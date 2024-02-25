// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for WallpaperSearchBackgroundManager.
class WallpaperSearchBackgroundManagerObserver : public base::CheckedObserver {
 public:
  // Called when the history is updated.
  virtual void OnHistoryUpdated() = 0;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_BACKGROUND_MANAGER_OBSERVER_H_
