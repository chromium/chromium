// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"

namespace wallpaper_handlers {

// A class to fetch SeaPen images from a Google server. Used for both Wallpaper
// and VC Background.
class SeaPenFetcher {
 public:
  using OnFetchThumbnailsComplete = base::OnceCallback<void(
      std::optional<std::vector<ash::SeaPenImage>> images,
      manta::MantaStatusCode status_code)>;
  using OnFetchWallpaperComplete =
      base::OnceCallback<void(std::optional<ash::SeaPenImage> image)>;

  // The number of thumbnails requested per call for text queries.
  constexpr static size_t kNumTextThumbnailsRequested = 4;

  // The number of thumbnails requested per call for template queries.
  constexpr static size_t kNumTemplateThumbnailsRequested = 8;

  // Timeout value for fetching SeaPen thumbnails and wallpaper. Requests that
  // take longer than this will return with an error instead of completing.
  constexpr static base::TimeDelta kRequestTimeout = base::Seconds(20);

  SeaPenFetcher();

  SeaPenFetcher(const SeaPenFetcher&) = delete;
  SeaPenFetcher& operator=(const SeaPenFetcher&) = delete;

  virtual ~SeaPenFetcher();

  // Run `query` against the Manta API. `query` is required to be a valid UTF-8
  // string no longer than `kMaximumGetSeaPenThumbnailsTextBytes`. Thumbnails
  // are decoded and re-encoded in a sandboxed process for safety before being
  // sent to the caller in `callback`.
  virtual void FetchThumbnails(
      manta::proto::FeatureName feature_name,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchThumbnailsComplete callback) = 0;

  // Calls the Manta API to fetch a higher resolution image of the thumbnail.
  // Wallpaper image is decoded and re-encoded in a sandboxed process for safety
  // before being sent to the caller in `callback`.
  virtual void FetchWallpaper(
      manta::proto::FeatureName feature_name,
      const ash::SeaPenImage& thumbnail,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchWallpaperComplete callback) = 0;

 private:
  // Allow delegate to view the constructor function.
  friend class WallpaperFetcherDelegateImpl;
  friend class SeaPenFetcherTest;

  // Private forces creation via `WallpaperFetcherDelegate` to set up mocking
  // in test code. `snapper_provider` may be null.
  static std::unique_ptr<SeaPenFetcher> MakeSeaPenFetcher(
      std::unique_ptr<manta::SnapperProvider> snapper_provider);
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
