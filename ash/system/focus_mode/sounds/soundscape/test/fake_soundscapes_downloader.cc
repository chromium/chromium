// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/test/fake_soundscapes_downloader.h"

#include "base/task/sequenced_task_runner.h"

namespace ash {

namespace {

constexpr char kTestHost[] = "https://www.example.com/path";

}  // namespace

FakeSoundscapesDownloader::FakeSoundscapesDownloader() = default;
FakeSoundscapesDownloader::~FakeSoundscapesDownloader() = default;

void FakeSoundscapesDownloader::SetPlaylistResponse(
    const SoundscapePlaylist& playlist) {
  // `SoundscapePlaylist` doesn't have a copy constructor because it's slow in
  // production. Manually copy for the test.
  SoundscapePlaylist copy;
  copy.uuid = playlist.uuid;
  copy.name = playlist.name;
  copy.thumbnail = playlist.thumbnail;
  copy.tracks = playlist.tracks;

  test_playlist_.emplace(std::move(copy));
}

void FakeSoundscapesDownloader::FetchConfiguration(
    ConfigurationCallback callback) {
  std::optional<SoundscapeConfiguration> configuration;
  if (test_playlist_) {
    configuration.emplace();
    configuration->playlists.push_back(std::move(*test_playlist_));
    test_playlist_.reset();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(configuration)));
}

GURL FakeSoundscapesDownloader::ResolveUrl(std::string_view path) {
  GURL test_url(kTestHost);
  return test_url.Resolve(path);
}

}  // namespace ash
