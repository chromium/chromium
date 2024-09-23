// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/wallpaper_metrics_provider.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"

namespace {

void OnTemplateIdFromFileExtracted(std::optional<int> template_id) {
  if (!template_id.has_value()) {
    return;
  }
  base::UmaHistogramSparse("Ash.Wallpaper.SeaPen.Template.Settled",
                           *template_id);
}

}  // namespace
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
  if (!info) {
    return;
  }

  if (ash::IsOnlineWallpaper(info->type)) {
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
    return;
  }

  if (info->type == ash::WallpaperType::kSeaPen) {
    uint32_t sea_pen_id;
    if (!base::StringToUint(info->location, &sea_pen_id)) {
      LOG(ERROR) << __func__ << "invalid key for Sea Pen wallpaper";
      return;
    }

    const AccountId account_id =
        ash::Shell::Get()->session_controller()->GetActiveAccountId();

    ash::SeaPenWallpaperManager::GetInstance()->GetTemplateIdFromFile(
        account_id, sea_pen_id, base::BindOnce(&OnTemplateIdFromFileExtracted));
  }
}
