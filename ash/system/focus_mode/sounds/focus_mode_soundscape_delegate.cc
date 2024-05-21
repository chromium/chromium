// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"

#include <vector>

#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscapes_downloader.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {

namespace {

// Length of time that we will retain a configuration before requesting a new
// one.
constexpr base::TimeDelta kCacheLifetime = base::Days(3);

FocusModeSoundsDelegate::Playlist ConvertPlaylist(
    const SoundscapePlaylist& playlist,
    SoundscapesDownloader* soundscapes_downloader) {
  const std::string& id = playlist.uuid.AsLowercaseString();
  const std::string& title = playlist.name;
  const GURL& thumbnail_url =
      soundscapes_downloader->ResolveUrl(playlist.thumbnail);

  return FocusModeSoundsDelegate::Playlist(id, title, thumbnail_url);
}

std::vector<FocusModeSoundsDelegate::Playlist> PlaylistsFromConfig(
    const SoundscapeConfiguration& configuration,
    SoundscapesDownloader* downloader) {
  std::vector<FocusModeSoundsDelegate::Playlist> requests;
  for (const auto& playlist : configuration.playlists) {
    requests.push_back(ConvertPlaylist(playlist, downloader));
  }
  return requests;
}

}  // namespace

FocusModeSoundscapeDelegate::FocusModeSoundscapeDelegate(
    const std::string& locale) {
  downloader_ = SoundscapesDownloader::Create(locale);
}

FocusModeSoundscapeDelegate::~FocusModeSoundscapeDelegate() = default;

bool FocusModeSoundscapeDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  // NOT IMPLEMENTED
  return false;
}

bool FocusModeSoundscapeDelegate::GetPlaylists(PlaylistsCallback callback) {
  if (cached_configuration_) {
    base::TimeDelta update_age = base::Time::Now() - last_update_;
    if (update_age < kCacheLifetime) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    PlaylistsFromConfig(*cached_configuration_,
                                                        downloader_.get())));
      return true;
    }
  }

  downloader_->FetchConfiguration(
      base::BindOnce(&FocusModeSoundscapeDelegate::HandleConfiguration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void FocusModeSoundscapeDelegate::HandleConfiguration(
    PlaylistsCallback callback,
    std::optional<SoundscapeConfiguration> configuration) {
  if (!configuration) {
    std::move(callback).Run({});
    return;
  }

  last_update_ = base::Time::Now();
  cached_configuration_ = std::move(configuration);

  std::move(callback).Run(
      PlaylistsFromConfig(*cached_configuration_, downloader_.get()));
}

}  // namespace ash
