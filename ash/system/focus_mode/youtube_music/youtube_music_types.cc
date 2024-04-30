// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace ash::youtube_music {

Image::Image(const int width, const int height, const GURL& url)
    : width(width), height(height), url(url) {}

Image::~Image() = default;

std::string Image::ToString() const {
  return base::StringPrintf("Image(width=%d, height=%d, url=%s)", width, height,
                            url.spec().c_str());
}

MusicSection::MusicSection(const std::string& name, const std::string& title)
    : name(name), title(title) {}

MusicSection::~MusicSection() = default;

std::string MusicSection::ToString() const {
  return base::StringPrintf("MusicSection(name=%s, title=%s)", name.c_str(),
                            title.c_str());
}

Playlist::Playlist(const std::string& name,
                   const std::string& title,
                   const std::string& owner_title,
                   const Image& image)
    : name(name), title(title), owner_title(owner_title), image(image) {}

Playlist::~Playlist() = default;

std::string Playlist::ToString() const {
  return base::StringPrintf(
      "Playlist(name=%s, title=%s, owner_title=%s, image=%s)", name.c_str(),
      title.c_str(), owner_title.c_str(), image.ToString().c_str());
}

PlaybackContext::PlaybackContext(const std::string& track_name,
                                 const std::string& track_title,
                                 const Image& track_image,
                                 const GURL& stream_url,
                                 const std::string& queue_name)
    : track_name(track_name),
      track_title(track_title),
      track_image(track_image),
      stream_url(stream_url),
      queue_name(queue_name) {}

PlaybackContext::~PlaybackContext() = default;

std::string PlaybackContext::ToString() const {
  return base::StringPrintf(
      "PlaybackContext(track_name=%s, track_title=%s, track_image=%s, "
      "stream_url=%s, queue_name=%s)",
      track_name.c_str(), track_title.c_str(), track_image.ToString().c_str(),
      stream_url.spec().c_str(), queue_name.c_str());
}

}  // namespace ash::youtube_music
