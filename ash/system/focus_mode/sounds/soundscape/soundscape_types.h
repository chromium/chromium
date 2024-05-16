// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPE_TYPES_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPE_TYPES_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/uuid.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

struct ASH_EXPORT SoundscapeTrack {
  // If possible, returns a fully populated `SoundscapeTrack` from `value`.
  // Otherwise, nullopt.
  static std::optional<SoundscapeTrack> FromValue(const base::Value& value);

  SoundscapeTrack(const std::string& name, const std::string& path);
  SoundscapeTrack(const SoundscapeTrack&);
  ~SoundscapeTrack();

  std::string name;

  // Relative path to the audio file. Relative to the configuration origin.
  std::string path;
};

struct ASH_EXPORT SoundscapePlaylist {
  // Returns a fully populated `SoundscapePlaylist` from `value` if all fields
  // are available. Uses the name for `locale` if found. Otherwise, names
  // default to "en-US". If `value` does not generate a fully populated
  // `SoundscapePlaylist`, returns nullptr.
  static std::optional<SoundscapePlaylist> FromValue(std::string_view locale,
                                                     const base::Value& value);

  SoundscapePlaylist();
  SoundscapePlaylist(const SoundscapePlaylist&) = delete;
  SoundscapePlaylist(SoundscapePlaylist&&);
  ~SoundscapePlaylist();

  // Uniquely identifies a playlist. Playlists which share a Uuid will have the
  // same contents.
  base::Uuid uuid;

  std::string name;

  // A relative path to the thumbnail image (relative to the origin url of
  // the configuration).
  std::string thumbnail;
  std::vector<SoundscapeTrack> tracks;
};

// Represents a configuration for Soundscapes consisting of a set of
// playlists which are made of a set of tracks which can be played while
// a user is in a Focus Mode session.
struct ASH_EXPORT SoundscapeConfiguration {
  // Attempts to build a `SoundscapeConfiguration` from `json` parsed as JSON.
  // If any fields are missing or invalid, returns nullopt.
  static std::optional<SoundscapeConfiguration> ParseConfiguration(
      std::string_view locale,
      std::string_view json);

  SoundscapeConfiguration();
  SoundscapeConfiguration(const SoundscapeConfiguration&) = delete;
  SoundscapeConfiguration(SoundscapeConfiguration&&);
  SoundscapeConfiguration& operator=(SoundscapeConfiguration&&);
  ~SoundscapeConfiguration();

  std::vector<SoundscapePlaylist> playlists;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPE_TYPES_H_
