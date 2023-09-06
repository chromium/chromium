// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_DAILY_REFRESH_SCHEDULER_H_
#define ASH_WALLPAPER_WALLPAPER_DAILY_REFRESH_SCHEDULER_H_

#include "ash/system/scheduled_feature/scheduled_feature.h"

#include "ash/public/cpp/wallpaper/wallpaper_info.h"

namespace ash {

// A scheduler that sends signal to WallpaperController whether the
// current's user wallpaper should be refreshed.
class WallpaperDailyRefreshScheduler : public ScheduledFeature {
 public:
  WallpaperDailyRefreshScheduler();

  WallpaperDailyRefreshScheduler(const WallpaperDailyRefreshScheduler& other) =
      delete;
  WallpaperDailyRefreshScheduler& operator=(
      const WallpaperDailyRefreshScheduler& rhs) = delete;
  ~WallpaperDailyRefreshScheduler() override = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Determines whether the current daily wallpaper should be refreshed.
  bool ShouldRefreshWallpaper(const WallpaperInfo& info);

 private:
  // ScheduledFeature:
  const char* GetFeatureName() const override;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_DAILY_REFRESH_SCHEDULER_H_
