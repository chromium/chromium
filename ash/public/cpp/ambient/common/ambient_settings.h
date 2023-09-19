// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
#define ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"

namespace ash {

// Structs and classes related to Ambient mode Settings.

ASH_PUBLIC_EXPORT extern const char kAmbientModeRecentHighlightsAlbumId[];
ASH_PUBLIC_EXPORT extern const char kAmbientModeFeaturedPhotoAlbumId[];
ASH_PUBLIC_EXPORT extern const char kAmbientModeFineArtAlbumId[];
ASH_PUBLIC_EXPORT extern const char kAmbientModeEarthAndSpaceAlbumId[];
ASH_PUBLIC_EXPORT extern const char kAmbientModeStreetArtAlbumId[];
ASH_PUBLIC_EXPORT extern const char kAmbientModeCapturedOnPixelAlbumId[];

// Subsettings of Art gallery.
struct ASH_PUBLIC_EXPORT ArtSetting {
  ArtSetting();
  ArtSetting(const ArtSetting&);
  ArtSetting(ArtSetting&&);
  ArtSetting& operator=(const ArtSetting&);
  ArtSetting& operator=(ArtSetting&&);
  ~ArtSetting();

  int setting_id = 0;

  // Album ID for Art category, used in Settings UI to select Art categories.
  std::string album_id;

  // Whether the setting is enabled in the Art gallery topic source.
  bool enabled = false;

  // Whether the setting is visible to the user. This is controlled by feature
  // parameters to allow remotely disabling certain categories.
  bool visible = false;

  // UTF-8 encoded.
  std::string title;

  // UTF-8 encoded.
  std::string description;

  std::string preview_image_url;
};

enum class AmbientModeTemperatureUnit {
  kMinValue = 0,
  kFahrenheit = kMinValue,
  kCelsius = 1,
  kMaxValue = kCelsius
};

struct ASH_PUBLIC_EXPORT AmbientSettings {
  AmbientSettings();
  AmbientSettings(const AmbientSettings&);
  AmbientSettings(AmbientSettings&&);
  AmbientSettings& operator=(const AmbientSettings&);
  AmbientSettings& operator=(AmbientSettings&&);
  ~AmbientSettings();

  personalization_app::mojom::TopicSource topic_source =
      personalization_app::mojom::TopicSource::kArtGallery;

  // Only a subset Settings of Art gallery.
  std::vector<ArtSetting> art_settings;

  // Only selected album.
  std::vector<std::string> selected_album_ids;

  // This setting is not exposed to the user as right now we are expecting
  // weather info to always show on ambient.
  bool show_weather = true;

  AmbientModeTemperatureUnit temperature_unit =
      AmbientModeTemperatureUnit::kFahrenheit;
};

struct ASH_PUBLIC_EXPORT PersonalAlbum {
  PersonalAlbum();
  PersonalAlbum(const PersonalAlbum&) = delete;
  PersonalAlbum(PersonalAlbum&&);
  PersonalAlbum& operator=(const PersonalAlbum&) = delete;
  PersonalAlbum& operator=(PersonalAlbum&&);
  ~PersonalAlbum();

  // ID of this album.
  std::string album_id;

  // UTF-8 encoded.
  std::string album_name;

  // Whether the album is selected in the Google Photos topic source.
  bool selected = false;

  // UTF-8 encoded.
  std::string description;

  int number_of_photos = 0;

  // Preview image of this album.
  std::string banner_image_url;

  // Preview images if this album is Recent highlights.
  std::vector<std::string> preview_image_urls;
};

struct ASH_PUBLIC_EXPORT PersonalAlbums {
  PersonalAlbums();
  PersonalAlbums(const PersonalAlbums&) = delete;
  PersonalAlbums(PersonalAlbums&&);
  PersonalAlbums& operator=(const PersonalAlbums&) = delete;
  PersonalAlbums& operator=(PersonalAlbums&&);
  ~PersonalAlbums();

  std::vector<PersonalAlbum> albums;

  // A token that the client application can use to retrieve the next batch of
  // albums. If the token is not set in the response, it means that there are
  // no more albums.
  std::string resume_token;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
