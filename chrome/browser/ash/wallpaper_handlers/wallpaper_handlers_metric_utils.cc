// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

namespace wallpaper_handlers {
namespace {

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
  if (!ash::features::IsSeaPenTextInputEnabled()) {
    const size_t limit = SeaPenFetcher::kNumTemplateThumbnailsRequested;
    base::UmaHistogramExactLinear(
        ToHistogramString(query_tag, SeaPenApiType::kThumbnails, "Count"),
        std::min(thumbnails_count, limit), limit + 1);
    return;
  }
  const size_t limit = SeaPenFetcher::kNumTextThumbnailsRequested;
  // The histogram name is different because when SeaPenTextInput is enabled,
  // template query will request 4 thumbnails instead of 8.
  //
  // Ash.SeaPen.Api.Thumbnails.Count: template thumbnails from 0-8.
  // Ash.SeaPen.Api.Thumbnails.Count2: template thumbnails from 0-4.
  // Ash.SeaPen.Freeform.Api.Thumbnails.Count: text thumbnails from 0-4.
  const std::string_view histogram_name =
      (query_tag ==
       ash::personalization_app::mojom::SeaPenQuery::Tag::kTextQuery)
          ? "Count"
          : "Count2";
  base::UmaHistogramExactLinear(
      ToHistogramString(query_tag, SeaPenApiType::kThumbnails, histogram_name),
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
