// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"

namespace ash {

FocusModeSoundsDelegate::Playlist::Playlist(const std::string& id,
                                            const std::string& title,
                                            const GURL& thumbnail_url)
    : id(id), title(title), thumbnail_url(thumbnail_url) {}
FocusModeSoundsDelegate::Playlist::Playlist(const Playlist&) = default;
FocusModeSoundsDelegate::Playlist& FocusModeSoundsDelegate::Playlist::operator=(
    const Playlist&) = default;
FocusModeSoundsDelegate::Playlist::~Playlist() = default;

FocusModeSoundsDelegate::Track::Track(const std::string& title,
                                      const std::string& artist,
                                      const std::string& source,
                                      const GURL& thumbnail_url,
                                      const GURL& source_url)
    : title(title),
      artist(artist),
      source(source),
      thumbnail_url(thumbnail_url),
      source_url(source_url) {}
FocusModeSoundsDelegate::Track::Track(const Track&) = default;
FocusModeSoundsDelegate::Track& FocusModeSoundsDelegate::Track::operator=(
    const Track&) = default;
FocusModeSoundsDelegate::Track::~Track() = default;

}  // namespace ash
