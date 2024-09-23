// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"

#include "base/strings/stringprintf.h"

namespace ash {

FocusModeSoundsDelegate::Playlist::Playlist(const std::string& id,
                                            const std::string& title,
                                            const GURL& thumbnail_url)
    : id(id), title(title), thumbnail_url(thumbnail_url) {}
FocusModeSoundsDelegate::Playlist::Playlist(const Playlist&) = default;
FocusModeSoundsDelegate::Playlist& FocusModeSoundsDelegate::Playlist::operator=(
    const Playlist&) = default;
FocusModeSoundsDelegate::Playlist::~Playlist() = default;

std::string FocusModeSoundsDelegate::Playlist::ToString() const {
  return base::StringPrintf(
      "Playlist(id=\"%s\", title=\"%s\", thumbnail_url=\"%s\")", id.c_str(),
      title.c_str(), thumbnail_url.spec().c_str());
}

FocusModeSoundsDelegate::Track::Track(const std::string& title,
                                      const std::string& artist,
                                      const std::string& source,
                                      const GURL& thumbnail_url,
                                      const GURL& source_url,
                                      const bool enable_playback_reporting)
    : title(title),
      artist(artist),
      source(source),
      thumbnail_url(thumbnail_url),
      source_url(source_url),
      enable_playback_reporting(enable_playback_reporting) {}
FocusModeSoundsDelegate::Track::Track(const Track&) = default;
FocusModeSoundsDelegate::Track& FocusModeSoundsDelegate::Track::operator=(
    const Track&) = default;
FocusModeSoundsDelegate::Track::~Track() = default;

std::string FocusModeSoundsDelegate::Track::ToString() const {
  return base::StringPrintf(
      "Track(title=\"%s\", artist=\"%s\", source=\"%s\", thumbnail_url=\"%s\", "
      "source_url=\"%s\")",
      title.c_str(), artist.c_str(), source.c_str(),
      thumbnail_url.spec().c_str(), source_url.spec().c_str());
}

}  // namespace ash
