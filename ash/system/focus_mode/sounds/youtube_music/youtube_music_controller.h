// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_client.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/containers/flat_map.h"

class AccountId;

namespace ash::youtube_music {

// Provides access to the YouTube Music API for the active account through
// active client.
class ASH_EXPORT YouTubeMusicController : public SessionObserver {
 public:
  YouTubeMusicController();
  YouTubeMusicController(const YouTubeMusicController&) = delete;
  YouTubeMusicController& operator=(const YouTubeMusicController&) = delete;
  ~YouTubeMusicController() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Returns the client for the active account.
  youtube_music::YouTubeMusicClient* GetActiveClient() const;

  // Triggers request to get music sections through the active client. Returns
  // true if the request is successfully triggered.
  bool GetMusicSection(youtube_music::GetMusicSectionCallback callback);

  // Triggers request to get specific playlist with name `playlist_id` through
  // the active client. Returns true if the request is successfully triggered.
  bool GetPlaylist(const std::string& playlist_id,
                   youtube_music::GetPlaylistCallback callback);

  // Triggers request to prepare the playback queue through the active client.
  // Returns true if the request is successfully triggered.
  bool PlaybackQueuePrepare(const std::string& playlist_id,
                            youtube_music::GetPlaybackContextCallback callback);

  // Triggers request to play the next track in the playback queue through the
  // active client. Returns true if the request is successfully triggered.
  bool PlaybackQueueNext(const std::string& playback_queue_id,
                         youtube_music::GetPlaybackContextCallback callback);

  // Triggers request to report the playback. Returns true if the request is
  // successfully triggered.
  bool ReportPlayback(const std::string& playback_reporting_token,
                      const PlaybackData& playback_data,
                      ReportPlaybackCallback callback);

 private:
  std::string GetDeviceId();

  base::flat_map<AccountId, std::unique_ptr<youtube_music::YouTubeMusicClient>>
      clients_;
};

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CONTROLLER_H_
