// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"
#include "components/account_id/account_id.h"

namespace ash::youtube_music {

// Interface for communicating with the Youtube Music API through the active
// client.
class ASH_EXPORT YoutubeMusicDelegate {
 public:
  YoutubeMusicDelegate() = default;
  YoutubeMusicDelegate(const YoutubeMusicDelegate&) = delete;
  YoutubeMusicDelegate& operator=(const YoutubeMusicDelegate&) = delete;
  virtual ~YoutubeMusicDelegate() = default;

  // Creates the API client for the active account.
  virtual YoutubeMusicRequestReturnCode CreateClientForActiveAccount(
      const AccountId& active_id) = 0;

  // Triggers request to get music sections through the active client.
  virtual YoutubeMusicRequestReturnCode GetMusicSections(
      GetMusicSectionsCallback callback) = 0;

  // Triggers request to get playlists through the active client.
  virtual YoutubeMusicRequestReturnCode GetPlaylists(
      const std::string& music_section_name,
      GetPlaylistsCallback callback) = 0;

  // Triggers request to prepare the playback queue through the active client.
  virtual YoutubeMusicRequestReturnCode PlaybackQueuePrepare(
      const std::string& playlist_name,
      GetPlaybackContextCallback callback) = 0;

  // Triggers request to play the next track in the playback queue through the
  // active client.
  virtual YoutubeMusicRequestReturnCode PlaybackQueueNext(
      const std::string& playback_queue_name,
      GetPlaybackContextCallback callback) = 0;
};

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_DELEGATE_H_
