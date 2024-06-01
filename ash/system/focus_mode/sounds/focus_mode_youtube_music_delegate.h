// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_controller.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"
#include "base/functional/callback.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

class ASH_EXPORT FocusModeYouTubeMusicDelegate
    : public FocusModeSoundsDelegate {
 public:
  FocusModeYouTubeMusicDelegate();
  ~FocusModeYouTubeMusicDelegate() override;

  // FocusModeSoundsDelegate:
  bool GetNextTrack(const std::string& playlist_id,
                    FocusModeSoundsDelegate::TrackCallback callback) override;
  bool GetPlaylists(
      FocusModeSoundsDelegate::PlaylistsCallback callback) override;

  void SetFailureCallback(base::RepeatingClosure callback);

 private:
  // Called when get playlists request is done.
  void OnGetPlaylistsDone(
      google_apis::ApiErrorCode http_error_code,
      std::optional<const std::vector<youtube_music::Playlist>> playlists);

  // Called when switching to next track is done.
  void OnNextTrackDone(
      google_apis::ApiErrorCode http_error_code,
      std::optional<const youtube_music::PlaybackContext> playback_context);

  // Cached callbacks for API requests.
  FocusModeSoundsDelegate::PlaylistsCallback get_playlists_callback_;
  FocusModeSoundsDelegate::TrackCallback get_next_track_callback_;

  // Last playlist/queue name requested through `GetNextTrack()`.
  std::string last_playlist_name_;
  std::string last_queue_name_;

  // Callback to run when the request fails.
  base::RepeatingClosure failure_callback_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
