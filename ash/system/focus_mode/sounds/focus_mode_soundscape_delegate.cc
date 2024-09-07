// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"

#include <optional>
#include <vector>

#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/sounds/soundscape/playlist_tracker.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscapes_downloader.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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

FocusModeSoundsDelegate::Track FromTrack(const SoundscapeTrack& track,
                                         const std::string& playlist_thumbnail,
                                         SoundscapesDownloader& resolver) {
  return FocusModeSoundsDelegate::Track(
      /*title=*/track.name,
      // `artist` is always empty for soundscapes.
      /*artist=*/"",
      /*source=*/"Focus Sounds",
      /*thumbnail_url*/ resolver.ResolveUrl(playlist_thumbnail),
      /*source_url=*/resolver.ResolveUrl(track.path),
      // Soundscapes does not require playback reporting.
      /*enable_playback_reporting=*/false);
}

}  // namespace

// static
std::unique_ptr<FocusModeSoundscapeDelegate>
FocusModeSoundscapeDelegate::Create(const std::string& locale) {
  return std::make_unique<FocusModeSoundscapeDelegate>(
      SoundscapesDownloader::Create(locale));
}

FocusModeSoundscapeDelegate::FocusModeSoundscapeDelegate(
    std::unique_ptr<SoundscapesDownloader> downloader)
    : downloader_(std::move(downloader)) {}

FocusModeSoundscapeDelegate::~FocusModeSoundscapeDelegate() {
  // Free the tracker before we release `cached_configuration_`.
  playlist_tracker_.reset();
}

void FocusModeSoundscapeDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  if (!cached_configuration_) {
    // TODO(b/342467806): Support fetching a configuration here.
    LOG(WARNING) << "Track requested before configuration download";

    // The callback must be invoked no matter what since the mojom
    // interface pipe is still waiting for response. Please see bug:
    // b/358625939.
    std::move(callback).Run(std::nullopt);

    return;
  }

  if (!playlist_tracker_ || playlist_id != playlist_tracker_->id()) {
    // When switching playlists, create a new tracker.
    const std::vector<SoundscapePlaylist>& playlists =
        cached_configuration_->playlists;
    auto iter =
        std::find_if(playlists.cbegin(), playlists.cend(),
                     [playlist_id](const SoundscapePlaylist& playlist) {
                       return playlist_id == playlist.uuid.AsLowercaseString();
                     });

    if (iter == playlists.end() || iter->tracks.empty()) {
      LOG(WARNING)
          << (iter == playlists.end()
                  ? "Could not find playlist in the cached configuration."
                  : "The playlist has no tracks.");

      // Must invoke the callback.
      std::move(callback).Run(std::nullopt);

      return;
    }

    const SoundscapePlaylist& playlist = *iter;
    playlist_tracker_.emplace(playlist);
  }

  const SoundscapeTrack& next_track = playlist_tracker_->NextTrack();
  std::optional<FocusModeSoundsDelegate::Track> track = FromTrack(
      next_track, playlist_tracker_->playlist().thumbnail, *downloader_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(track)));
}

void FocusModeSoundscapeDelegate::GetPlaylists(PlaylistsCallback callback) {
  if (cached_configuration_) {
    base::TimeDelta update_age = base::Time::Now() - last_update_;
    if (update_age < kCacheLifetime) {
      // Return the cached playlists.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    PlaylistsFromConfig(*cached_configuration_,
                                                        downloader_.get())));
      return;
    }
  }

  // Configuration is outdated. Clear it.
  playlist_tracker_.reset();
  cached_configuration_.reset();

  downloader_->FetchConfiguration(
      base::BindOnce(&FocusModeSoundscapeDelegate::HandleConfiguration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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
