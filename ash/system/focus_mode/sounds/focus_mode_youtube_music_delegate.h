// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"

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
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
