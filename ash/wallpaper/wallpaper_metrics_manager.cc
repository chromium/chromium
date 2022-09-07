// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_metrics_manager.h"

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

WallpaperMetricsManager::WallpaperMetricsManager() {
  wallpaper_controller_observation_.Observe(WallpaperController::Get());
}

WallpaperMetricsManager::~WallpaperMetricsManager() = default;

void WallpaperMetricsManager::OnOnlineWallpaperSet(
    const OnlineWallpaperParams& params) {
  if (params.from_user) {
    const absl::optional<uint64_t>& unit_id = params.unit_id;
    DCHECK(unit_id.has_value());
    const int unit_id_val = unit_id.value();
    base::UmaHistogramSparse("Ash.Wallpaper.Image", unit_id_val);

    const std::string& collection_id = params.collection_id;
    DCHECK(!collection_id.empty());
    const int collection_id_hash = base::PersistentHash(collection_id);
    base::UmaHistogramSparse("Ash.Wallpaper.Collection", collection_id_hash);
  }
}

void WallpaperMetricsManager::OnWallpaperChanged() {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Wallpaper.Type",
      Shell::Get()->wallpaper_controller()->GetWallpaperType(),
      WallpaperType::kCount);
}

void WallpaperMetricsManager::OnWallpaperPreviewStarted() {
  base::UmaHistogramBoolean("Ash.Wallpaper.Preview.Show", true);
}

}  // namespace ash
