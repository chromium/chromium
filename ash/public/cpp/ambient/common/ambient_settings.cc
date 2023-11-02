// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/common/ambient_settings.h"

namespace ash {

const char kAmbientModeRecentHighlightsAlbumId[] = "RecentHighlights";
const char kAmbientModeFeaturedPhotoAlbumId[] = "FeaturedPhoto";
const char kAmbientModeFineArtAlbumId[] = "FineArt";
const char kAmbientModeEarthAndSpaceAlbumId[] = "EarthAndSpace";
const char kAmbientModeStreetArtAlbumId[] = "StreetArt";
const char kAmbientModeCapturedOnPixelAlbumId[] = "CapturedOnPixel";

// ArtSetting ------------------------------------------------------------------

ArtSetting::ArtSetting() = default;

ArtSetting::ArtSetting(const ArtSetting&) = default;

ArtSetting::ArtSetting(ArtSetting&&) = default;

ArtSetting& ArtSetting::operator=(const ArtSetting&) = default;

ArtSetting& ArtSetting::operator=(ArtSetting&&) = default;

ArtSetting::~ArtSetting() = default;

// AmbientSettings -------------------------------------------------------------

AmbientSettings::AmbientSettings() = default;

AmbientSettings::AmbientSettings(const AmbientSettings&) = default;

AmbientSettings::AmbientSettings(AmbientSettings&&) = default;

AmbientSettings& AmbientSettings::operator=(const AmbientSettings&) = default;

AmbientSettings& AmbientSettings::operator=(AmbientSettings&&) = default;

AmbientSettings::~AmbientSettings() = default;

// PersonalAlbum ---------------------------------------------------------------

PersonalAlbum::PersonalAlbum() = default;

PersonalAlbum::PersonalAlbum(PersonalAlbum&&) = default;

PersonalAlbum& PersonalAlbum::operator=(PersonalAlbum&&) = default;

PersonalAlbum::~PersonalAlbum() = default;

// PersonalAlbums --------------------------------------------------------------

PersonalAlbums::PersonalAlbums() = default;

PersonalAlbums::PersonalAlbums(PersonalAlbums&&) = default;

PersonalAlbums& PersonalAlbums::operator=(PersonalAlbums&&) = default;

PersonalAlbums::~PersonalAlbums() = default;

}  // namespace ash
