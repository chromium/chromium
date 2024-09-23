// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_TIME_OF_DAY_SCHEDULER_H_
#define ASH_WALLPAPER_WALLPAPER_TIME_OF_DAY_SCHEDULER_H_

#include "ash/system/scheduled_feature/scheduled_feature.h"

class PrefRegistrySimple;

namespace ash {

// Only applies to the time-of-day wallpaper collection. This is always stuck
// on the `kSunsetToSunrise` schedule and dictates when the time-of-day
// wallpaper changes. All other wallpaper collections follow D/L mode's
// schedule settings.
class WallpaperTimeOfDayScheduler : public ScheduledFeature {
 public:
  WallpaperTimeOfDayScheduler();
  WallpaperTimeOfDayScheduler(const WallpaperTimeOfDayScheduler& other) =
      delete;
  WallpaperTimeOfDayScheduler& operator=(
      const WallpaperTimeOfDayScheduler& rhs) = delete;
  ~WallpaperTimeOfDayScheduler() override = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // ScheduledFeature:
  const char* GetFeatureName() const override;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_TIME_OF_DAY_SCHEDULER_H_
