// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_UTIL_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_UTIL_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"

namespace google_apis::youtube_music {
class Image;
class Playlist;
class TopLevelMusicRecommendations;
class Queue;
}  // namespace google_apis::youtube_music

namespace ash::youtube_music {

struct Image;
struct Playlist;
struct PlaybackContext;

// Gets `Image` from API image. When `api_image` is null, it returns an empty
// image.
ASH_EXPORT Image
GetImageFromApiImage(const google_apis::youtube_music::Image* api_image);

// Gets a vector of `Image` from API images. When `api_image` is null, it
// returns an empty vector.
ASH_EXPORT std::vector<Image> GetImagesFromApiImages(
    const std::vector<std::unique_ptr<google_apis::youtube_music::Image>>&
        api_images);

// Gets `Playlist` from API playlist. When `playlist` is null, it returns
// nullopt.
ASH_EXPORT Playlist GetPlaylistFromApiPlaylist(
    const google_apis::youtube_music::Playlist& playlist);

// Gets a vector of `Playlist` from API top level music recommendations.
ASH_EXPORT std::vector<Playlist>
GetPlaylistsFromApiTopLevelMusicRecommendations(
    const google_apis::youtube_music::TopLevelMusicRecommendations&
        top_level_music_recommendations);

// Gets `PlaybackContext` from API queue.
ASH_EXPORT PlaybackContext
GetPlaybackContextFromApiQueue(const google_apis::youtube_music::Queue& queue);

// Returns the best image to use. It uses the smallest qualified image possible;
// if no qualified images, it uses the biggest image.
ASH_EXPORT Image FindBestImage(const std::vector<Image>& images);

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_UTIL_H_
