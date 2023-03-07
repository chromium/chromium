// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_ONLINE_WALLPAPER_VARIANT_INFO_FETCHER_H_
#define ASH_WALLPAPER_ONLINE_WALLPAPER_VARIANT_INFO_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

namespace backdrop {
class Image;
}

namespace ash {

class WallpaperControllerClient;

// Resolves wallpaper variants from WallpaperInfo for WallpaperController.
class ASH_EXPORT OnlineWallpaperVariantInfoFetcher {
 public:
  enum class ColorMode {
    kDarkMode = 0,
    kLightMode,
  };

  OnlineWallpaperVariantInfoFetcher();

  OnlineWallpaperVariantInfoFetcher(const OnlineWallpaperVariantInfoFetcher&) =
      delete;
  OnlineWallpaperVariantInfoFetcher& operator=(
      const OnlineWallpaperVariantInfoFetcher&) = delete;

  ~OnlineWallpaperVariantInfoFetcher();

  void SetClient(WallpaperControllerClient* client);

  // Callback for Fetch* methods which populates the |unit_id| and |variants|
  // fields in OnlineWallpaperParams.
  using FetchParamsCallback =
      base::OnceCallback<void(absl::optional<OnlineWallpaperParams>)>;

  // Fetches the wallpaper variants for |info| to produce a fully populated
  // OnlineWallpaperParams in |callback|. The selected wallpaper will is
  // designated by asset_id in |info|.
  void FetchOnlineWallpaper(const AccountId& account_id,
                            const WallpaperInfo& info,
                            OnlineWallpaperVariantInfoFetcher::ColorMode mode,
                            FetchParamsCallback callback);

  // Always fetches a new daily refresh wallpaper and calls |callback| with a
  // fully populated OnlineWallpaperParams.
  bool FetchDailyWallpaper(const AccountId& account_id,
                           const WallpaperInfo& info,
                           OnlineWallpaperVariantInfoFetcher::ColorMode mode,
                           FetchParamsCallback callback);

 private:
  // An internal representation of the partial information required to construct
  // a complete OnlineWallpaperParams object as provided by the caller of
  // Fetch*.
  class OnlineWallpaperRequest {
   public:
    OnlineWallpaperRequest(const AccountId& account_id,
                           const std::string& collection_id,
                           WallpaperLayout layout,
                           bool daily_refresh_enabled,
                           OnlineWallpaperVariantInfoFetcher::ColorMode mode);
    OnlineWallpaperRequest(const OnlineWallpaperRequest&) = delete;
    OnlineWallpaperRequest& operator=(const OnlineWallpaperRequest&) = delete;
    ~OnlineWallpaperRequest();

    AccountId account_id;
    std::string collection_id;
    WallpaperLayout layout;
    bool daily_refresh_enabled;
    OnlineWallpaperVariantInfoFetcher::ColorMode mode;
  };

  // Handles the response for a single random image in a collection and proceeds
  // to fetch the rest of the collection.
  void OnSingleFetch(std::unique_ptr<OnlineWallpaperRequest> request,
                     FetchParamsCallback callback,
                     bool success,
                     const backdrop::Image& image);

  // Finishes variants fetch by populating the remaining fields for
  // OnlineWallpaperParams in |callback|. Combines data from |request| with
  // |images| and the matching variant in |images| for |asset_id|.
  void FindAndSetOnlineWallpaperVariants(
      std::unique_ptr<OnlineWallpaperRequest> request,
      uint64_t asset_id,
      FetchParamsCallback callback,
      bool success,
      const std::vector<backdrop::Image>& images);

  raw_ptr<WallpaperControllerClient> wallpaper_controller_client_ =
      nullptr;  // not owned

  base::WeakPtrFactory<OnlineWallpaperVariantInfoFetcher> weak_factory_{this};
};

}  // namespace ash

#endif  //  ASH_WALLPAPER_ONLINE_WALLPAPER_VARIANT_INFO_FETCHER_H_
