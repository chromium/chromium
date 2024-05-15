// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {

namespace {

std::vector<FocusModeSoundsDelegate::Playlist> SoundscapeData() {
  std::vector<FocusModeSoundsDelegate::Playlist> playlists(
      {{"playlists/soundscapemusic0", "Chill R&B",
        GURL("https://music.soundscape.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/soundscapemusic1", "Unwind Test Long Name",
        GURL("https://music.soundscape.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/soundscapemusic2", "Velvet Voices",
        GURL("https://music.soundscape.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")},
       {"playlists/soundscapemusic3", "Lofi Loft",
        GURL("https://music.soundscape.com/image/"
             "mixart?r="
             "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4")}});
  return playlists;
}

}  // namespace

FocusModeSoundscapeDelegate::FocusModeSoundscapeDelegate() = default;
FocusModeSoundscapeDelegate::~FocusModeSoundscapeDelegate() = default;

bool FocusModeSoundscapeDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  // NOT IMPLEMENTED
  return false;
}

bool FocusModeSoundscapeDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SoundscapeData()));
  return true;
}

}  // namespace ash
