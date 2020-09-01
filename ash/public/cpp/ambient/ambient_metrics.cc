// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_metrics.h"

#include "ash/public/cpp/ambient/common/ambient_settings.h"

namespace ash {
namespace ambient {

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

}  // namespace ambient
}  // namespace ash
