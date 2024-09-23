// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_

#include <optional>
#include <string>

#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/time/time.h"
#include "components/manta/manta_status.h"

namespace wallpaper_handlers {

// Enumeration of Google Photos API endpoints.
enum class GooglePhotosApi {
  kGetAlbum,
  kGetAlbums,
  kGetEnabled,
  kGetPhoto,
  kGetPhotos,
  kGetSharedAlbums,
};

// Records the following on Google Photos API response parsing completion:
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.ResponseTime.[Success|Failure]
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.Result
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.Result.Count
// NOTE: success/failure is assumed by the presence/absence of `result_count`.
void RecordGooglePhotosApiResponseParsed(GooglePhotosApi api,
                                         base::TimeDelta response_time,
                                         std::optional<size_t> result_count);

// Records Ash.Wallpaper.GooglePhotos.Api.{Api}.RefreshCount metric.
void RecordGooglePhotosApiRefreshCount(GooglePhotosApi api, int refresh_count);

// Used to record metrics for SeaPen initial thumbnails request and SeaPen
// upscale request. Keep in sync with histograms.xml variant SeaPenApiType.
enum class SeaPenApiType {
  kThumbnails,
  kWallpaper,
};

// Records the client side latency of an API request. Only record if the request
// completed successfully and did not timeout.
void RecordSeaPenLatency(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    base::TimeDelta elapsed_time,
    SeaPenApiType sea_pen_sea_pen_api_type);

// Records the status code of an API request before any client side modification
// (e.g. client changes status code to kGenericError if the response is missing
// images).
void RecordSeaPenMantaStatusCode(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    manta::MantaStatusCode status_code,
    SeaPenApiType sea_pen_sea_pen_api_type);

// Records whether the request timed out.
void RecordSeaPenTimeout(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    bool hit_timeout,
    SeaPenApiType sea_pen_sea_pen_api_type);

// Records the number of thumbnails returned. Only recorded if the request
// completed successfully. Expected to be in bounds [0,
// kNumThumbnailsRequested].
void RecordSeaPenThumbnailsCount(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    size_t thumbnails_count);

// Records whether at least one image exists on the response for full size
// wallpaper image. Only recorded if the request completed successfully.
void RecordSeaPenWallpaperHasImage(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    bool has_image);

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_
