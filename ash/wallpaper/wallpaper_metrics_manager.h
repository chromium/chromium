// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_METRICS_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_METRICS_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/scoped_observation.h"

namespace ash {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SetWallpaperResult {
  kSuccess = 0,
  kPermissionDenied,
  kNetworkError,
  kDecodingError,
  kInvalidState,
  kRequestFailure,
  kFileNotFound,
  kMaxValue = kFileNotFound,
};

// The implementation of WallpaperControllerObserver that saves metrics.
class ASH_EXPORT WallpaperMetricsManager : public WallpaperControllerObserver {
 public:
  WallpaperMetricsManager();

  WallpaperMetricsManager(const WallpaperMetricsManager&) = delete;
  WallpaperMetricsManager& operator=(const WallpaperMetricsManager&) = delete;

  ~WallpaperMetricsManager() override;

  static std::string ToResultHistogram(WallpaperType type);

  // WallpaperControllerObserver:
  void OnOnlineWallpaperSet(const OnlineWallpaperParams& params) override;
  void OnWallpaperChanged() override;
  void OnWallpaperPreviewStarted() override;

  void LogSettingTimeOfDayWallpaperAfterOobe(bool success);
  void LogWallpaperResult(WallpaperType type, SetWallpaperResult reason);

 private:
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_METRICS_MANAGER_H_
