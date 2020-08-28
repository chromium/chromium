// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_metrics.h"

#include <string>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {
namespace ambient {

namespace {

// 144 == 24 * 60 / 10. Each histogram bucket therefore represents 10 minutes.
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
                                  bool tablet_mode) {
  base::UmaHistogramCustomTimes(
      /*name=*/GetHistogramName("Ash.AmbientMode.EngagementTime", tablet_mode),
      /*sample=*/time_delta,
      /*min=*/base::TimeDelta::FromHours(0),
      /*max=*/base::TimeDelta::FromHours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);
}

}  // namespace ambient
}  // namespace ash
