// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_metrics_manager.h"

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace ash {

WallpaperMetricsManager::WallpaperMetricsManager() {
  wallpaper_controller_observation_.Observe(WallpaperController::Get());
}

WallpaperMetricsManager::~WallpaperMetricsManager() = default;

// static
// NOTE: These strings are persisted to metric logs.
std::string WallpaperMetricsManager::ToResultHistogram(WallpaperType type) {
  switch (type) {
    case WallpaperType::kOnline:
      return "Ash.Wallpaper.Online.Result2";
    case WallpaperType::kCustomized:
      return "Ash.Wallpaper.Customized.Result2";
    case WallpaperType::kOnceGooglePhotos:
      return "Ash.Wallpaper.OnceGooglePhotos.Result2";
    case WallpaperType::kDaily:
      return "Ash.Wallpaper.Daily.Result2";
    case WallpaperType::kDailyGooglePhotos:
      return "Ash.Wallpaper.DailyGooglePhotos.Result2";
    case WallpaperType::kPolicy:
      return "Ash.Wallpaper.Policy.Result2";
    case WallpaperType::kThirdParty:
      return "Ash.Wallpaper.ThirdParty.Result2";
    case WallpaperType::kDefault:
      return "Ash.Wallpaper.Default.Result2";
    case WallpaperType::kDevice:
      return "Ash.Wallpaper.Device.Result2";
    case WallpaperType::kOobe:
      return "Ash.Wallpaper.Oobe.Result2";
    case WallpaperType::kSeaPen:
      return "Ash.Wallpaper.SeaPen.Result2";
    case WallpaperType::kOneShot:
    case WallpaperType::kCount:
      NOTREACHED();
  }
}

void WallpaperMetricsManager::OnOnlineWallpaperSet(
    const OnlineWallpaperParams& params) {
  if (params.from_user) {
    const std::optional<uint64_t>& unit_id = params.unit_id;
    DCHECK(unit_id.has_value());
    const int unit_id_val = unit_id.value();
    base::UmaHistogramSparse("Ash.Wallpaper.Image", unit_id_val);

    const std::string& collection_id = params.collection_id;
    DCHECK(!collection_id.empty());
    const int collection_id_hash = base::PersistentHash(collection_id);
    DVLOG(3) << __PRETTY_FUNCTION__ << " collection_id=" << collection_id
             << " hash=" << collection_id_hash;
    base::UmaHistogramSparse("Ash.Wallpaper.Collection", collection_id_hash);
  }
}

void WallpaperMetricsManager::OnWallpaperChanged() {
  base::UmaHistogramEnumeration(
      "Ash.Wallpaper.Type",
      Shell::Get()->wallpaper_controller()->GetWallpaperType(),
      WallpaperType::kCount);
}

void WallpaperMetricsManager::OnWallpaperPreviewStarted() {
  base::UmaHistogramBoolean("Ash.Wallpaper.Preview.Show", true);
}

void WallpaperMetricsManager::LogSettingTimeOfDayWallpaperAfterOobe(
    bool success) {
  base::UmaHistogramBoolean("Ash.Wallpaper.IsSetToTimeOfDayAfterOobe", success);
}

void WallpaperMetricsManager::LogWallpaperResult(WallpaperType type,
                                                 SetWallpaperResult result) {
  base::UmaHistogramEnumeration(ToResultHistogram(type), result);
}

}  // namespace ash
