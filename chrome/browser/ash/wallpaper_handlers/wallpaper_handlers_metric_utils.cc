// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

namespace wallpaper_handlers {
namespace {

// NOTE: These strings are persisted to metric logs.
std::string ToHistogramBase(GooglePhotosApi api) {
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
std::string ToHistogramString(SeaPenApiType sea_pen_api_type) {
  switch (sea_pen_api_type) {
    case SeaPenApiType::kThumbnails:
      return "Thumbnails";
    case SeaPenApiType::kWallpaper:
      return "Wallpaper";
  }
}

}  // namespace

// NOTE: Histogram names are persisted to metric logs.
void RecordGooglePhotosApiResponseParsed(GooglePhotosApi api,
                                         base::TimeDelta response_time,
                                         std::optional<size_t> result_count) {
  const std::string histogram_base = ToHistogramBase(api);
  const bool success = result_count.has_value();

  // Record response time.
  base::UmaHistogramTimes(
      base::StringPrintf("%s.ResponseTime.%s", histogram_base.c_str(),
                         success ? "Success" : "Failure"),
      response_time);

  // Record result.
  base::UmaHistogramBoolean(
      base::StringPrintf("%s.Result", histogram_base.c_str()), success);

  // Record result count.
  if (result_count.has_value()) {
    base::UmaHistogramCounts1000(
        base::StringPrintf("%s.Result.Count", histogram_base.c_str()),
        result_count.value());
  }
}

void RecordGooglePhotosApiRefreshCount(GooglePhotosApi api, int refresh_count) {
  // Record refresh count.
  const std::string histogram_base = ToHistogramBase(api);
  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.RefreshCount", histogram_base.c_str()),
      refresh_count, 11);
}

void RecordSeaPenLatency(const base::TimeDelta elapsed_time,
                         const SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramCustomTimes(
      base::StringPrintf("Ash.SeaPen.Api.%s.Latency",
                         ToHistogramString(sea_pen_api_type).c_str()),
      elapsed_time,
      /*min=*/base::Seconds(1),
      /*max=*/SeaPenFetcher::kRequestTimeout,
      /*buckets=*/50);
}

void RecordSeaPenMantaStatusCode(const manta::MantaStatusCode status_code,
                                 const SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramEnumeration(
      base::StringPrintf("Ash.SeaPen.Api.%s.MantaStatusCode",
                         ToHistogramString(sea_pen_api_type).c_str()),
      status_code);
}

void RecordSeaPenTimeout(bool hit_timeout, SeaPenApiType sea_pen_api_type) {
  base::UmaHistogramBoolean(
      base::StringPrintf("Ash.SeaPen.Api.%s.Timeout",
                         ToHistogramString(sea_pen_api_type).c_str()),
      hit_timeout);
}

void RecordSeaPenThumbnailsCount(const size_t thumbnails_count) {
  base::UmaHistogramExactLinear(
      "Ash.SeaPen.Api.Thumbnails.Count",
      std::min(thumbnails_count, SeaPenFetcher::kNumThumbnailsRequested),
      SeaPenFetcher::kNumThumbnailsRequested + 1);
}

void RecordSeaPenWallpaperHasImage(bool has_image) {
  base::UmaHistogramBoolean("Ash.SeaPen.Api.Wallpaper.HasImage", has_image);
}

}  // namespace wallpaper_handlers
