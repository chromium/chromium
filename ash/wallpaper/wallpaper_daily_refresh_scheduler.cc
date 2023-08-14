// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace {

// 6:00 PM. The primary checkpoint to signal whether the daily wallpaper
// should be refreshed.
constexpr int kFirstCheckpointOffsetMinutes = 18 * 60;
// 7:00 PM. The secondary checkpoint to serve as a retry in case the wallpaper
// wasn't refreshed successfully when the first checkpoint is fired.
constexpr int kSecondCheckpointOffsetMinutes = 19 * 60;

}  // namespace

WallpaperDailyRefreshScheduler::WallpaperDailyRefreshScheduler()
    : ScheduledFeature(prefs::kWallpaperDailyRefreshCheck,
                       prefs::kWallpaperDailyRefreshScheduleType,
                       prefs::kWallpaperDailyRefreshFirstCheckTime,
                       prefs::kWallpaperDailyRefreshSecondCheckTime) {}

// static
void WallpaperDailyRefreshScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshScheduleType,
                                static_cast<int>(ScheduleType::kCustom));
  registry->RegisterBooleanPref(prefs::kWallpaperDailyRefreshCheck, false);
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshFirstCheckTime,
                                kFirstCheckpointOffsetMinutes);
  registry->RegisterIntegerPref(prefs::kWallpaperDailyRefreshSecondCheckTime,
                                kSecondCheckpointOffsetMinutes);
}

const char* WallpaperDailyRefreshScheduler::GetFeatureName() const {
  return "WallpaperDailyRefreshScheduler";
}

}  // namespace ash
