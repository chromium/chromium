// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/wallpaper_metrics_provider.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"

WallpaperMetricsProvider::WallpaperMetricsProvider() = default;
WallpaperMetricsProvider::~WallpaperMetricsProvider() = default;

void WallpaperMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!ash::Shell::HasInstance() ||
      !ash::Shell::Get()->wallpaper_controller()) {
    return;
  }

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  auto info = wallpaper_controller->GetActiveUserWallpaperInfo();
  if (!info || !ash::IsOnlineWallpaper(info->type)) {
    return;
  }
  base::UmaHistogramBoolean("Ash.Wallpaper.Image.Settled.HasUnitId",
                            info->unit_id.has_value());
  if (info->unit_id.has_value()) {
    base::UmaHistogramSparse("Ash.Wallpaper.Image.Settled",
                             info->unit_id.value());
  }
  base::UmaHistogramBoolean("Ash.Wallpaper.Image.Settled.HasCollectionId",
                            !info->collection_id.empty());
  if (!info->collection_id.empty()) {
    const int collection_id_hash = base::PersistentHash(info->collection_id);
    base::UmaHistogramSparse("Ash.Wallpaper.Collection.Settled",
                             collection_id_hash);
  }
}
