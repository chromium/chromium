// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
#define ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"

namespace ash::youtube_music {

// Interface for the Youtube Music client.
class ASH_EXPORT YoutubeMusicClient {
 public:
  YoutubeMusicClient() = default;
  YoutubeMusicClient(const YoutubeMusicClient&) = delete;
  YoutubeMusicClient& operator=(const YoutubeMusicClient&) = delete;
  virtual ~YoutubeMusicClient() = default;

  virtual void GetMusicSections(GetMusicSectionsCallback callback) = 0;

  virtual void GetPlaylists(const std::string& music_section_name,
                            GetPlaylistsCallback callback) = 0;

  virtual void PreparePlaybackQueue(const std::string& playlist_name,
                                    GetPlaybackContextCallback callback) = 0;

  virtual void NextPlaybackQueue(const std::string& playback_queue_name,
                                 GetPlaybackContextCallback callback) = 0;
};

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
