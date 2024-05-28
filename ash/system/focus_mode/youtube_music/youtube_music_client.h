// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
#define ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"

namespace ash::youtube_music {

// Interface for the YouTube Music client.
class ASH_EXPORT YouTubeMusicClient {
 public:
  // Callback for creating an instance of `google_apis::RequestSender` for the
  // client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  explicit YouTubeMusicClient(
      const CreateRequestSenderCallback& create_request_sender_callback);
  YouTubeMusicClient(const YouTubeMusicClient&) = delete;
  YouTubeMusicClient& operator=(const YouTubeMusicClient&) = delete;
  ~YouTubeMusicClient();

  // Invokes a request to the API server for playlist data.
  void GetPlaylists(GetPlaylistsCallback callback);

  // Invokes a request to the API server for preparing the playback queue.
  void PlaybackQueuePrepare(const std::string& playlist_name,
                            GetPlaybackContextCallback callback);

  // Invokes a request to the API server for requesting the next track in in the
  // playback queue.
  void PlaybackQueueNext(const std::string& playback_queue_name,
                         GetPlaybackContextCallback callback);

 private:
  google_apis::RequestSender* GetRequestSender();

  // Triggered when playlists are fetched.
  void OnGetPlaylistsRequestDone(
      const base::Time& request_start_time,
      base::expected<
          std::unique_ptr<
              google_apis::youtube_music::TopLevelMusicRecommendations>,
          google_apis::ApiErrorCode> result);

  // Triggered when play context is fetched by preparing the playback queue.
  void OnPlaybackQueuePrepareRequestDone(
      const base::Time& request_start_time,
      base::expected<std::unique_ptr<google_apis::youtube_music::Queue>,
                     google_apis::ApiErrorCode> result);

  // Triggered when play context is fetched by requesting next in the playback
  // queue.
  void OnPlaybackQueueNextRequestDone(
      const base::Time& request_start_time,
      base::expected<
          std::unique_ptr<google_apis::youtube_music::QueueContainer>,
          google_apis::ApiErrorCode> result);

  // Callback passed in at initialization time for creating request sender.
  CreateRequestSenderCallback create_request_sender_callback_;

  // Callback that runs when play list data is fetched.
  GetPlaylistsCallback playlists_callback_;

  // Callback that runs when playback context data is fetched by preparing the
  // playback queue.
  GetPlaybackContextCallback playback_context_prepare_callback_;

  // Callback that runs when playback context data is fetched by requesting next
  // in the playback queue.
  GetPlaybackContextCallback playback_context_next_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  base::WeakPtrFactory<YouTubeMusicClient> weak_factory_{this};
};

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
