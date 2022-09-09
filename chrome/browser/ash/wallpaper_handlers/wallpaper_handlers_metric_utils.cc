// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

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
  }
}

}  // namespace

// NOTE: Histogram names are persisted to metric logs.
void RecordGooglePhotosApiResponseParsed(GooglePhotosApi api,
                                         base::TimeDelta response_time,
                                         absl::optional<size_t> result_count) {
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

}  // namespace wallpaper_handlers
