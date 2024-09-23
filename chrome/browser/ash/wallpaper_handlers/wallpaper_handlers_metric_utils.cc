// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

namespace wallpaper_handlers {
namespace {

// NOTE: These strings are persisted to metric logs.
std::string_view ToHistogramBase(GooglePhotosApi api) {
  switch (api) {
    case GooglePhotosApi::kGetAlbum:
      return "Ash.Wallpaper.GooglePhotos.Api.GetAlbum";
    case GooglePhotosApi::kGetAlbums:
      return "Ash.Wallpaper.GooglePhotos.Api.GetAlbums";
    case GooglePhotosApi::kGetEnabled:
      return "Ash.Wallpaper.GooglePhotos.Api.GetEnabled";
    case GooglePhotosApi::kGetPhoto:
      return "Ash.Wallpaper.GooglePhotos.Api.GetPhoto";
    case GooglePhotosApi::kGetPhotos:
      return "Ash.Wallpaper.GooglePhotos.Api.GetPhotos";
    case GooglePhotosApi::kGetSharedAlbums:
      return "Ash.Wallpaper.GooglePhotos.Api.GetSharedAlbums";
  }
}

// NOTE: These strings are persisted to metric logs and should match
// SeaPenApiType variants in
// //tools/metrics/histograms/metadata/ash/histograms.xml.
std::string ToHistogramString(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    SeaPenApiType sea_pen_api_type,
    std::string_view histogram_name) {
  const bool is_text_query =
      query_tag ==
      ash::personalization_app::mojom::SeaPenQuery::Tag::kTextQuery;
  const std::string_view freeform = is_text_query ? "Freeform." : "";
  const bool for_thumbnails = sea_pen_api_type == SeaPenApiType::kThumbnails;
  const std::string api_type = for_thumbnails ? "Thumbnails." : "Wallpaper.";
  return base::StrCat(
      {"Ash.SeaPen.", freeform, "Api.", api_type, histogram_name});
}

}  // namespace

// NOTE: Histogram names are persisted to metric logs.
void RecordGooglePhotosApiResponseParsed(GooglePhotosApi api,
                                         base::TimeDelta response_time,
                                         std::optional<size_t> result_count) {
  const std::string_view histogram_base = ToHistogramBase(api);
  const bool success = result_count.has_value();
  base::UmaHistogramTimes(base::StrCat({histogram_base, ".ResponseTime.",
                                        success ? "Success" : "Failure"}),
                          response_time);
  base::UmaHistogramBoolean(base::StrCat({histogram_base, ".Result"}), success);
  if (success) {
    base::UmaHistogramCounts1000(
        base::StrCat({histogram_base, ".Result.Count"}), result_count.value());
  }
}

void RecordGooglePhotosApiRefreshCount(GooglePhotosApi api, int refresh_count) {
  // Record refresh count.
  base::UmaHistogramExactLinear(
      base::StrCat({ToHistogramBase(api), ".RefreshCount"}), refresh_count, 11);
}

void RecordSeaPenLatency(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    base::TimeDelta elapsed_time,
    SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramCustomTimes(
      ToHistogramString(query_tag, sea_pen_api_type, "Latency"), elapsed_time,
      /*min=*/base::Seconds(1),
      /*max=*/SeaPenFetcher::kRequestTimeout,
      /*buckets=*/50);
}

void RecordSeaPenMantaStatusCode(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    manta::MantaStatusCode status_code,
    SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramEnumeration(
      ToHistogramString(query_tag, sea_pen_api_type, "MantaStatusCode"),
      status_code);
}

void RecordSeaPenTimeout(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    bool hit_timeout,
    SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramBoolean(
      ToHistogramString(query_tag, sea_pen_api_type, "Timeout"), hit_timeout);
}

void RecordSeaPenThumbnailsCount(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    size_t thumbnails_count) {
  const size_t limit =
      (query_tag ==
       ash::personalization_app::mojom::SeaPenQuery::Tag::kTextQuery)
          ? SeaPenFetcher::kNumTextThumbnailsRequested
          : SeaPenFetcher::kNumTemplateThumbnailsRequested;
  base::UmaHistogramExactLinear(
      ToHistogramString(query_tag, SeaPenApiType::kThumbnails, "Count"),
      std::min(thumbnails_count, limit), limit + 1);
}

void RecordSeaPenWallpaperHasImage(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    bool has_image) {
  base::UmaHistogramBoolean(
      ToHistogramString(query_tag, SeaPenApiType::kWallpaper, "HasImage"),
      has_image);
}

}  // namespace wallpaper_handlers
