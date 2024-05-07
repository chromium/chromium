// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_TYPES_H_
#define ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_TYPES_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/models/list_model.h"
#include "url/gurl.h"

namespace ash::youtube_music {

// For better aesthetics after resizing, the image sizes should be 2x as large
// as the UI requirements.
inline constexpr int kImageMinimalWidth = 72 * 2;
inline constexpr int kImageMinimalHeight = 72 * 2;

// Lightweight data structure definition to separate API and ash/ui-friendly
// types. It contains information that describes a single image. Details about
// the values can be found at:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/Image
struct ASH_EXPORT Image {
  Image(const int width, const int height, const GURL& url);
  ~Image();

  std::string ToString() const;

  int width;

  int height;

  GURL url;
};

// Lightweight data structure definition to separate API and ash/ui-friendly
// types. It contains information that describes a single music section. Details
// about the values can be found at:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load#MusicSection
struct ASH_EXPORT MusicSection {
  MusicSection(const std::string& name, const std::string& title);
  ~MusicSection();

  std::string ToString() const;

  const std::string name;

  const std::string title;
};

// Lightweight data structure definition to separate API and ash/ui-friendly
// types. It contains information that describes a single playlist. Details
// about the values can be found at:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/playlists#Playlist
struct ASH_EXPORT Playlist {
  Playlist(const std::string& name,
           const std::string& title,
           const std::string& owner_title,
           const Image& image);
  ~Playlist();

  std::string ToString() const;

  std::string name;

  std::string title;

  std::string owner_title;

  Image image;
};

// Lightweight data structure definition to separate API and ash/ui-friendly
// types. It contains information that describes a single playback context.
// Details about the values can be found at:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#Queue
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#PlaybackContext
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#QueueItem
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#PlaybackManifest
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#Stream
struct ASH_EXPORT PlaybackContext {
  PlaybackContext(const std::string& track_name,
                  const std::string& track_title,
                  const Image& track_image,
                  const GURL& stream_url,
                  const std::string& queue_name);
  ~PlaybackContext();

  std::string ToString() const;

  std::string track_name;

  std::string track_title;

  Image track_image;

  GURL stream_url;

  std::string queue_name;
};

using GetPlaylistsCallback =
    base::OnceCallback<void(google_apis::ApiErrorCode http_error_code,
                            const std::vector<Playlist> playlists)>;

using GetPlaybackContextCallback =
    base::OnceCallback<void(google_apis::ApiErrorCode http_error_code,
                            const PlaybackContext playback_context)>;

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_TYPES_H_
