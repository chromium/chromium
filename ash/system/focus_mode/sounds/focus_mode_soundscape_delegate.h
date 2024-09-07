// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDSCAPE_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDSCAPE_DELEGATE_H_

#include <string>
#include <utility>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/sounds/soundscape/playlist_tracker.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ash {

class SoundscapesDownloader;

class ASH_EXPORT FocusModeSoundscapeDelegate : public FocusModeSoundsDelegate {
 public:
  static std::unique_ptr<FocusModeSoundscapeDelegate> Create(
      const std::string& locale);

  explicit FocusModeSoundscapeDelegate(
      std::unique_ptr<SoundscapesDownloader> downloader);
  ~FocusModeSoundscapeDelegate() override;

  // FocusModeSoundsDelegate:
  void GetNextTrack(const std::string& playlist_id,
                    FocusModeSoundsDelegate::TrackCallback callback) override;
  void GetPlaylists(
      FocusModeSoundsDelegate::PlaylistsCallback callback) override;

 private:
  void HandleConfiguration(
      FocusModeSoundsDelegate::PlaylistsCallback callback,
      std::optional<SoundscapeConfiguration> configuration);

  base::Time last_update_;

  std::optional<PlaylistTracker> playlist_tracker_;

  std::optional<SoundscapeConfiguration> cached_configuration_;
  std::unique_ptr<SoundscapesDownloader> downloader_;

  base::WeakPtrFactory<FocusModeSoundscapeDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDSCAPE_DELEGATE_H_
