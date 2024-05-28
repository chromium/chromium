// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr size_t kPlaylistNum = 4;

youtube_music::YouTubeMusicController* GetYouTubeMusicConTroller() {
  if (auto* focus_mode_controller = FocusModeController::Get()) {
    return focus_mode_controller->youtube_music_controller();
  }
  return nullptr;
}
}  // namespace

FocusModeYouTubeMusicDelegate::FocusModeYouTubeMusicDelegate() = default;
FocusModeYouTubeMusicDelegate::~FocusModeYouTubeMusicDelegate() = default;

bool FocusModeYouTubeMusicDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  if (auto* youtube_music_controller = GetYouTubeMusicConTroller()) {
    if (last_playlist_name_ != playlist_id) {
      get_next_track_callback_ = std::move(callback);
      last_playlist_name_ = playlist_id;
      youtube_music_controller->PlaybackQueuePrepare(
          playlist_id,
          base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                         base::Unretained(this)));
    } else {
      get_next_track_callback_ = std::move(callback);
      youtube_music_controller->PlaybackQueueNext(
          last_queue_name_,
          base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                         base::Unretained(this)));
    }
    return true;
  }

  return false;
}

bool FocusModeYouTubeMusicDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  if (auto* youtube_music_controller = GetYouTubeMusicConTroller()) {
    get_playlists_callback_ = std::move(callback);
    youtube_music_controller->GetPlaylists(
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnGetPlaylistsDone,
                       base::Unretained(this)));
    return true;
  }

  return false;
}

void FocusModeYouTubeMusicDelegate::OnGetPlaylistsDone(
    google_apis::ApiErrorCode http_error_code,
    std::optional<const std::vector<youtube_music::Playlist>> playlists) {
  if (!get_playlists_callback_ || !playlists.has_value()) {
    return;
  }

  std::vector<Playlist> result;
  CHECK_GE(playlists.value().size(), kPlaylistNum);
  for (size_t i = 0; i < kPlaylistNum; i++) {
    result.emplace_back(playlists.value()[i].name, playlists.value()[i].title,
                        playlists.value()[i].image.url);
  }
  std::move(get_playlists_callback_).Run(result);
}

void FocusModeYouTubeMusicDelegate::OnNextTrackDone(
    google_apis::ApiErrorCode http_error_code,
    std::optional<const youtube_music::PlaybackContext> playback_context) {
  if (!get_next_track_callback_ || !playback_context.has_value()) {
    return;
  }

  last_queue_name_ = playback_context->queue_name;

  Track result(/*title=*/playback_context->track_title, /*artist=*/"",
               /*source=*/"",
               /*thumbnail_url=*/playback_context->track_image.url,
               /*source_url=*/playback_context->stream_url);
  std::move(get_next_track_callback_).Run(result);
}

}  // namespace ash
