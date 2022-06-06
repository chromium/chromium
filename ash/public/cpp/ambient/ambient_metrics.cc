// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_metrics.h"

#include <string>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace ambient {

namespace {

// Histograms default to exponential bucketing, so the smallest bucket occupies
// 24 hours / (2 ^ (144 - 1)) milliseconds. Exponential bucketing is desirable
// for engagement time because most users exit screensaver on the order of
// several minutes, while a small fraction of users exit screensaver after
// many hours. So the histogram's highest resolution should occupy the smaller
// engagement times.
constexpr int kAmbientModeElapsedTimeHistogramBuckets = 144;

std::string GetHistogramName(const char* prefix, bool tablet_mode) {
  std::string histogram = prefix;
  if (tablet_mode)
    histogram += ".TabletMode";
  else
    histogram += ".ClamshellMode";

  return histogram;
}

}  // namespace

AmbientModePhotoSource AmbientSettingsToPhotoSource(
    const AmbientSettings& settings) {
  if (settings.topic_source == ash::AmbientModeTopicSource::kArtGallery)
    return AmbientModePhotoSource::kArtGallery;

  if (settings.selected_album_ids.size() == 0)
    return AmbientModePhotoSource::kGooglePhotosEmpty;

  bool has_recent_highlights = std::any_of(
      settings.selected_album_ids.cbegin(), settings.selected_album_ids.cend(),
      [](const std::string& album_id) {
        return album_id == ash::kAmbientModeRecentHighlightsAlbumId;
      });

  if (has_recent_highlights && settings.selected_album_ids.size() == 1)
    return AmbientModePhotoSource::kGooglePhotosRecentHighlights;

  if (has_recent_highlights && settings.selected_album_ids.size() > 1)
    return AmbientModePhotoSource::kGooglePhotosBoth;

  return AmbientModePhotoSource::kGooglePhotosPersonalAlbum;
}

void RecordAmbientModeActivation(AmbientUiMode ui_mode, bool tablet_mode) {
  base::UmaHistogramEnumeration(
      GetHistogramName("Ash.AmbientMode.Activation", tablet_mode), ui_mode);
}

void RecordAmbientModeTimeElapsed(base::TimeDelta time_delta,
                                  bool tablet_mode,
                                  AmbientAnimationTheme theme) {
  base::UmaHistogramCustomTimes(
      /*name=*/GetHistogramName("Ash.AmbientMode.EngagementTime", tablet_mode),
      /*sample=*/time_delta,
      /*min=*/base::Hours(0),
      /*max=*/base::Hours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);

  base::UmaHistogramCustomTimes(
      /*name=*/base::StrCat(
          {"Ash.AmbientMode.EngagementTime.", ToString(theme)}),
      /*sample=*/time_delta,
      // There is no value in bucketing engagement times that are on the order
      // of milliseconds. A 1 second minimum is imposed here but not in the
      // metric above for legacy reasons (the metric above was already pushed
      // to the field and established before this change was made).
      /*min=*/base::Seconds(1),
      /*max=*/base::Hours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);
}

void RecordAmbientModeTotalNumberOfAlbums(int num_albums) {
  base::UmaHistogramCounts100("Ash.AmbientMode.TotalNumberOfAlbums",
                              num_albums);
}

void RecordAmbientModeSelectedNumberOfAlbums(int num_albums) {
  base::UmaHistogramCounts100("Ash.AmbientMode.SelectedNumberOfAlbums",
                              num_albums);
}

void RecordAmbientModeAnimationSmoothness(int smoothness,
                                          AmbientAnimationTheme theme) {
  base::UmaHistogramPercentage(
      base::StrCat(
          {"Ash.AmbientMode.LottieAnimationSmoothness.", ToString(theme)}),
      smoothness);
}

void RecordAmbientModePhotoOrientationMatch(int percentage_match,
                                            AmbientAnimationTheme theme) {
  base::UmaHistogramPercentage(
      base::StrCat({"Ash.AmbientMode.PhotoOrientationMatch.", ToString(theme)}),
      percentage_match);
}

}  // namespace ambient
}  // namespace ash
