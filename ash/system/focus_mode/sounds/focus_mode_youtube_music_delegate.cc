// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "url/gurl.h"

namespace ash {

namespace {

std::vector<FocusModeSoundsDelegate::Playlist> YouTubeMusicData() {
  std::vector<FocusModeSoundsDelegate::Playlist> playlists(
      {{"playlists/youtubemusic0", "Chill R&B",
        GURL("https://music.youtube.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/youtubemusic1", "Unwind Test Long Name",
        GURL("https://music.youtube.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/youtubemusic2", "Velvet Voices",
        GURL("https://music.youtube.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/youtubemusic3", "Lofi Loft",
        GURL("https://music.youtube.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")}});
  return playlists;
}

}  // namespace

FocusModeYouTubeMusicDelegate::FocusModeYouTubeMusicDelegate() = default;
FocusModeYouTubeMusicDelegate::~FocusModeYouTubeMusicDelegate() = default;

bool FocusModeYouTubeMusicDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  // NOT IMPLEMENTED
  return false;
}

bool FocusModeYouTubeMusicDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), YouTubeMusicData()));
  return true;
}

}  // namespace ash
