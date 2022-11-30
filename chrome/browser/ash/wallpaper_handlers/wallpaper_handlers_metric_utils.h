// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace wallpaper_handlers {

// Enumeration of Google Photos API endpoints.
enum class GooglePhotosApi {
  kGetAlbum,
  kGetAlbums,
  kGetEnabled,
  kGetPhoto,
  kGetPhotos,
};

// Records the following on Google Photos API response parsing completion:
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.ResponseTime.[Success|Failure]
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.Result
// * Ash.Wallpaper.GooglePhotos.Api.{Api}.Result.Count
// NOTE: success/failure is assumed by the presence/absence of `result_count`.
void RecordGooglePhotosApiResponseParsed(GooglePhotosApi api,
                                         base::TimeDelta response_time,
                                         absl::optional<size_t> result_count);

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_METRIC_UTILS_H_
