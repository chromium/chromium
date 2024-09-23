// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_TEST_FAKE_SOUNDSCAPES_DOWNLOADER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_TEST_FAKE_SOUNDSCAPES_DOWNLOADER_H_

#include <optional>

#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscapes_downloader.h"

namespace ash {

class ASH_EXPORT FakeSoundscapesDownloader : public SoundscapesDownloader {
 public:
  FakeSoundscapesDownloader();
  ~FakeSoundscapesDownloader() override;

  // Designate `playlist` to be returned in the `FetchConfiguration()` call.
  void SetPlaylistResponse(const SoundscapePlaylist& playlist);

  void FetchConfiguration(ConfigurationCallback callback) override;
  GURL ResolveUrl(std::string_view path) override;

 private:
  std::optional<SoundscapePlaylist> test_playlist_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_TEST_FAKE_SOUNDSCAPES_DOWNLOADER_H_
