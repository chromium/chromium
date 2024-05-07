// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/youtube_music/youtube_music_client.h"

namespace ash::youtube_music {

YouTubeMusicClient::YouTubeMusicClient(
    const CreateRequestSenderCallback& create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

YouTubeMusicClient::~YouTubeMusicClient() = default;

void YouTubeMusicClient::GetPlaylists(const std::string& music_section_name,
                                      GetPlaylistsCallback callback) {
  // TODO(yongshun): Start the request with retry.
}

void YouTubeMusicClient::PlaybackQueuePrepare(
    const std::string& playlist_name,
    GetPlaybackContextCallback callback) {
  // TODO(yongshun): Start the request with retry.
}

void YouTubeMusicClient::PlaybackQueueNext(
    const std::string& playback_queue_name,
    GetPlaybackContextCallback callback) {
  // TODO(yongshun): Start the request with retry.
}

google_apis::RequestSender* YouTubeMusicClient::GetRequestSender() {
  // TODO(yongshun): Create the request sender once OAuth2 scope is added to
  // gaia constants.

  return request_sender_.get();
}

}  // namespace ash::youtube_music
