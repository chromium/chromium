// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace ash {

namespace {

// Maximum allowed length of parsed strings.
constexpr int kStringMax = 255;

// Maximum number of tracks per playlist.
constexpr int kMaxTracks = 100;

// For the UI, there should always be 4 playlists in a valid configuration.
constexpr int kNumPlaylists = 4;

constexpr char kDefaultLocale[] = "en-US";

bool ValidString(const std::string* str) {
  return !!str && !str->empty() && str->length() <= kStringMax;
}

bool ValidList(const base::Value::List* list) {
  return !!list && !list->empty();
}

// Returns the best name for `locale` in `localized_names` which is assumed to
// be a Dict with a "locale" and "name" field. If a match for `locale` is not
// found, returns the result for the default locale (en-US). If a suitable name
// is not found, nullptr is returned.
const std::string* BestLocalizedName(std::string_view locale,
                                     const base::Value::List& localized_names) {
  const std::string* default_name = nullptr;
  const std::string* language_match = nullptr;

  for (const base::Value& localized_name : localized_names) {
    const base::Value::Dict* dict = localized_name.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* candidate_locale = dict->FindString("locale");
    if (!candidate_locale) {
      continue;
    }
    const std::string* candidate_name = dict->FindString("name");
    if (!ValidString(candidate_name)) {
      continue;
    }

    std::string_view locale_view(*candidate_locale);
    if (locale_view == locale) {
      // Exact matches are best. We're done.
      return candidate_name;
    }

    if (locale_view.substr(0, 2) == locale.substr(0, 2)) {
      // The first 2 letters of the locale represent the language. Try to match
      // this in the case that there is no exact match.
      language_match = candidate_name;
      continue;
    }

    if (locale_view == kDefaultLocale) {
      default_name = candidate_name;
      continue;
    }
  }

  if (language_match) {
    return language_match;
  }

  return default_name;
}

}  // namespace

// static
std::optional<SoundscapeTrack> SoundscapeTrack::FromValue(
    const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }

  const std::string* name = dict->FindString("name");
  const std::string* path = dict->FindString("path");

  if (!ValidString(name) || !ValidString(path)) {
    return std::nullopt;
  }

  return SoundscapeTrack(*name, *path);
}

SoundscapeTrack::SoundscapeTrack(const std::string& name,
                                 const std::string& path)
    : name(name), path(path) {}
SoundscapeTrack::SoundscapeTrack(const SoundscapeTrack&) = default;
SoundscapeTrack::~SoundscapeTrack() = default;

// static
std::optional<SoundscapePlaylist> SoundscapePlaylist::FromValue(
    std::string_view locale,
    const base::Value& value) {
  if (locale.length() != 2 && locale.length() != 5) {
    // Locales are either 2 or 5 characters representing just the language (e.g.
    // "en") or the language and country with a dash ("en-US") as described in
    // RFC 5646.
    return std::nullopt;
  }

  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }

  const base::Value::List* localized_names = dict->FindList("name");
  const std::string* thumbnail = dict->FindString("thumbnail");
  const std::string* uuid = dict->FindString("uuid");
  const base::Value::List* tracks = dict->FindList("tracks");

  if (!ValidList(localized_names) || !ValidList(tracks) ||
      !ValidString(thumbnail) || !ValidString(uuid)) {
    return std::nullopt;
  }

  if (tracks->size() > kMaxTracks) {
    LOG(WARNING) << "Too many tracks in playlist " << tracks->size();
    return std::nullopt;
  }

  const std::string* name = BestLocalizedName(locale, *localized_names);
  if (!name) {
    return std::nullopt;
  }

  base::Uuid parsed_uuid = base::Uuid::ParseLowercase(*uuid);
  if (!parsed_uuid.is_valid()) {
    return std::nullopt;
  }

  SoundscapePlaylist playlist;
  playlist.tracks.reserve(tracks->size());
  for (const base::Value& track : *tracks) {
    std::optional<SoundscapeTrack> parsed_track =
        SoundscapeTrack::FromValue(track);
    if (!parsed_track) {
      LOG(WARNING) << "Track validation failed";
      return std::nullopt;
    }
    playlist.tracks.push_back(std::move(*parsed_track));
  }

  playlist.name = *name;
  playlist.uuid = parsed_uuid;
  playlist.thumbnail = *thumbnail;

  return playlist;
}

SoundscapePlaylist::SoundscapePlaylist() = default;
SoundscapePlaylist::SoundscapePlaylist(SoundscapePlaylist&&) = default;
SoundscapePlaylist::~SoundscapePlaylist() = default;

SoundscapeConfiguration::SoundscapeConfiguration() = default;
SoundscapeConfiguration::SoundscapeConfiguration(SoundscapeConfiguration&&) =
    default;
SoundscapeConfiguration& SoundscapeConfiguration::operator=(
    SoundscapeConfiguration&&) = default;
SoundscapeConfiguration::~SoundscapeConfiguration() = default;

// static
std::optional<SoundscapeConfiguration>
SoundscapeConfiguration::ParseConfiguration(std::string_view locale,
                                            std::string_view json) {
  if (locale.size() != 2u && locale.size() != 5u) {
    LOG(ERROR) << "Invalid locale string";
    return std::nullopt;
  }

  if (json.empty()) {
    return std::nullopt;
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!value) {
    LOG(WARNING) << "Configuration cannot be parsed";
    return std::nullopt;
  }

  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict) {
    LOG(WARNING) << "Configuration is not a dictionary";
    return std::nullopt;
  }

  const base::Value::List* playlists = dict->FindList("playlists");
  // We always expect exactly 4 playlists.
  if (!playlists || playlists->size() != kNumPlaylists) {
    LOG(WARNING) << "Playlists are invalid";
    return std::nullopt;
  }

  SoundscapeConfiguration config;
  config.playlists.reserve(kNumPlaylists);
  for (const base::Value& playlist : *playlists) {
    std::optional<SoundscapePlaylist> parsed =
        SoundscapePlaylist::FromValue(locale, playlist);
    if (!parsed) {
      LOG(WARNING) << "Playlist validation failed";
      return std::nullopt;
    }
    config.playlists.push_back(std::move(*parsed));
  }

  return config;
}

}  // namespace ash
