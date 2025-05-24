// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_util.h"

#include <algorithm>

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"

namespace ash::youtube_music {

Image GetImageFromApiImage(const google_apis::youtube_music::Image* api_iamge) {
  Image image;
  if (api_iamge) {
    image.width = api_iamge->width();
    image.height = api_iamge->height();
    image.url = api_iamge->url();
  }
  return image;
}

std::vector<Image> GetImagesFromApiImages(
    const std::vector<std::unique_ptr<google_apis::youtube_music::Image>>&
        api_images) {
  std::vector<Image> images;
  for (const auto& api_image : api_images) {
    images.emplace_back(GetImageFromApiImage(api_image.get()));
  }
  return images;
}

Playlist GetPlaylistFromApiPlaylist(
    const google_apis::youtube_music::Playlist& playlist) {
  return Playlist(playlist.name(), playlist.title(), playlist.owner().title(),
                  FindBestImage(GetImagesFromApiImages(playlist.images())));
}

std::vector<Playlist> GetPlaylistsFromApiTopLevelMusicRecommendations(
    const google_apis::youtube_music::TopLevelMusicRecommendations&
        top_level_music_recommendations) {
  std::vector<Playlist> playlists;
  for (auto& top_level_recommendation :
       top_level_music_recommendations.top_level_music_recommendations()) {
    for (auto& music_recommendation :
         top_level_recommendation->music_section().music_recommendations()) {
      auto& playlist = music_recommendation->playlist();
      playlists.emplace_back(
          playlist.name(), playlist.title(), playlist.owner().title(),
          FindBestImage(GetImagesFromApiImages(playlist.images())));
    }
  }
  return playlists;
}

PlaybackContext GetPlaybackContextFromApiQueue(
    const google_apis::youtube_music::Queue& queue) {
  const auto& playback_context = queue.playback_context();
  const auto& track = playback_context.queue_item().track();
  // TODO(yongshun): Consider to add retry when there is no stream in the
  // response.
  GURL stream_url = GURL();
  std::string playback_reporting_token;
  if (auto& streams = playback_context.playback_manifest().streams();
      !streams.empty()) {
    const auto* stream = streams.begin()->get();
    stream_url = stream->url();
    playback_reporting_token = stream->playback_reporting_token();
  }

  std::string track_artists;
  for (size_t i = 0; i < track.artist_references().size(); i++) {
    track_artists += (i ? ", " : "") + track.artist_references()[i]->title();
  }

  return PlaybackContext(track.name(), track.title(), track_artists,
                         track.explicit_type(),
                         FindBestImage(GetImagesFromApiImages(track.images())),
                         stream_url, playback_reporting_token, queue.name());
}

Image FindBestImage(const std::vector<Image>& images) {
  if (images.empty()) {
    return Image();
  }

  auto smaller_in_size = [](const Image& img1, const Image& img2) {
    return img1.width * img1.height < img2.width * img2.height;
  };
  auto qualified = [](const Image& img) {
    return img.width >= kImageMinimalWidth && img.height >= kImageMinimalHeight;
  };

  size_t smallest_qualified_index = images.size();
  for (size_t i = 0; i < images.size(); i++) {
    if (qualified(images[i]) &&
        (smallest_qualified_index == images.size() ||
         smaller_in_size(images[i], images[smallest_qualified_index]))) {
      smallest_qualified_index = i;
    }
  }

  return smallest_qualified_index < images.size()
             ? images[smallest_qualified_index]
             : *std::max_element(images.begin(), images.end(), smaller_in_size);
}

}  // namespace ash::youtube_music
