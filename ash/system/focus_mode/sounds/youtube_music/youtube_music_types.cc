// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "url/gurl.h"

namespace ash::youtube_music {

Image::Image() : Image(0, 0, GURL()) {}

Image::Image(const int width, const int height, const GURL& url)
    : width(width), height(height), url(url) {}

Image::~Image() = default;

std::string Image::ToString() const {
  return base::StringPrintf("Image(width=%d, height=%d, url=\"%s\")", width,
                            height, url.spec().c_str());
}

MusicSection::MusicSection(const std::string& name, const std::string& title)
    : name(name), title(title) {}

MusicSection::~MusicSection() = default;

std::string MusicSection::ToString() const {
  return base::StringPrintf("MusicSection(name=\"%s\", title=\"%s\")",
                            name.c_str(), title.c_str());
}

Playlist::Playlist(const std::string& name,
                   const std::string& title,
                   const std::string& owner_title,
                   const Image& image)
    : name(name), title(title), owner_title(owner_title), image(image) {}

Playlist::Playlist(const Playlist& other) = default;

Playlist::~Playlist() = default;

std::string Playlist::ToString() const {
  return base::StringPrintf(
      "Playlist(name=\"%s\", title=\"%s\", owner_title=\"%s\", image=%s)",
      name.c_str(), title.c_str(), owner_title.c_str(),
      image.ToString().c_str());
}

PlaybackContext::PlaybackContext(const std::string& track_name,
                                 const std::string& track_title,
                                 const std::string& track_artists,
                                 const std::string& track_explicit_type,
                                 const Image& track_image,
                                 const GURL& stream_url,
                                 const std::string& playback_reporting_token,
                                 const std::string& queue_name)
    : track_name(track_name),
      track_title(track_title),
      track_artists(track_artists),
      track_explicit_type_(track_explicit_type),
      track_image(track_image),
      stream_url(stream_url),
      playback_reporting_token(playback_reporting_token),
      queue_name(queue_name) {}

PlaybackContext::PlaybackContext(const PlaybackContext& other) = default;

PlaybackContext::~PlaybackContext() = default;

std::string PlaybackContext::ToString() const {
  return base::StringPrintf(
      "PlaybackContext(track_name=\"%s\", track_title=\"%s\", "
      "track_artists=\"%s\", track_explicit_type=\"%s\", track_image=%s, "
      "stream_url=\"%s\", playback_reporting_token=\"%s\", queue_name=\"%s\")",
      track_name.c_str(), track_title.c_str(), track_artists.c_str(),
      track_explicit_type_.c_str(), track_image.ToString().c_str(),
      stream_url.spec().c_str(), playback_reporting_token.c_str(),
      queue_name.c_str());
}

MediaSegment::MediaSegment(int media_start,
                           int media_end,
                           const base::Time client_start_time)
    : media_start(media_start),
      media_end(media_end),
      client_start_time(client_start_time) {}

MediaSegment::MediaSegment(const MediaSegment&) = default;

MediaSegment& MediaSegment::operator=(const MediaSegment&) = default;

MediaSegment::~MediaSegment() = default;

std::string MediaSegment::ToString() const {
  return base::StringPrintf(
      "MediaSegment(media_start=%d, media_end=%d, client_start_time=\"%s\")",
      media_start, media_end,
      base::TimeFormatAsIso8601(client_start_time).c_str());
}

PlaybackData::PlaybackData(const PlaybackState state,
                           const std::string& title,
                           const GURL& url,
                           const base::Time client_current_time,
                           int playback_start_offset,
                           int media_time_current,
                           const MediaSegments& media_segments,
                           bool initial_playback)
    : state(state),
      title(title),
      url(url),
      client_current_time(client_current_time),
      playback_start_offset(playback_start_offset),
      media_time_current(media_time_current),
      media_segments(media_segments),
      initial_playback(initial_playback) {}

PlaybackData::PlaybackData(const PlaybackData&) = default;

PlaybackData& PlaybackData::operator=(const PlaybackData&) = default;

PlaybackData::~PlaybackData() = default;

std::string PlaybackData::ToString() const {
  std::string segments_str;
  for (auto it = media_segments.begin(); it != media_segments.end(); it++) {
    segments_str += (it == media_segments.begin() ? "" : ", ") + it->ToString();
  }
  return base::StringPrintf(
      "PlaybackData(state=%d, title=\"%s\", url=\"%s\", "
      "client_current_time=\"%s\", playback_start_offset=%d, "
      "media_time_current=%d, media_segments=[%s], initial_playback=%d)",
      state, title.c_str(), url.spec().c_str(),
      base::TimeFormatAsIso8601(client_current_time).c_str(),
      playback_start_offset, media_time_current, segments_str.c_str(),
      initial_playback);
}

bool PlaybackData::CanAggregateWithNewData(const PlaybackData& new_data) const {
  return url == new_data.url;
}

void PlaybackData::AggregateWithNewData(const PlaybackData& new_data) {
  CHECK(CanAggregateWithNewData(new_data));
  state = new_data.state;
  client_current_time = new_data.client_current_time;
  playback_start_offset = new_data.playback_start_offset;
  media_time_current = new_data.media_time_current;
  for (const auto& media_segment : new_data.media_segments) {
    media_segments.insert(media_segment);
  }
  initial_playback |= new_data.initial_playback;
}

}  // namespace ash::youtube_music
