// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "base/rand_util.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

WallpaperDailyRefreshScheduler::WallpaperDailyRefreshScheduler()
    : ScheduledFeature(prefs::kWallpaperDailyRefreshCheck,
                       prefs::kWallpaperDailyRefreshScheduleType,
                       prefs::kWallpaperDailyRefreshFirstCheckTime,
                       prefs::kWallpaperDailyRefreshSecondCheckTime) {}

// static
void WallpaperDailyRefreshScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Randomize the checkpoint time to prevent server load spikes.
  // First time is between 12:00am and 10:59pm and the second time is an hour
  // from the first.
  const int kFirstCheckpointOffsetMinutes = base::RandInt(0, 23 * 60 - 1);
  const int kSecondCheckpointOffsetMinutes = kFirstCheckpointOffsetMinutes + 60;
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshScheduleType,
                                static_cast<int>(ScheduleType::kCustom));
  registry->RegisterBooleanPref(prefs::kWallpaperDailyRefreshCheck, false);
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshFirstCheckTime,
                                kFirstCheckpointOffsetMinutes);
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshSecondCheckTime,
                                kSecondCheckpointOffsetMinutes);
}

bool WallpaperDailyRefreshScheduler::ShouldRefreshWallpaper(
    const WallpaperInfo& info) {
  if (info.type != WallpaperType::kDaily &&
      info.type != WallpaperType::kDailyGooglePhotos) {
    return false;
  }
  // When `features::IsWallpaperFastRefreshEnabled()` is enabled, the
  // wallpaper may swap quickly back to back due to how ScheduledFeature
  // stabilizes its schedule state.
  return features::IsWallpaperFastRefreshEnabled()
             ? true
             : info.date + base::Hours(23) <= Now();
}

const char* WallpaperDailyRefreshScheduler::GetFeatureName() const {
  return "WallpaperDailyRefreshScheduler";
}

}  // namespace ash
