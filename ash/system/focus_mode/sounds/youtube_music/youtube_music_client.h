// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/youtube_music/request_signer.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/youtube_music/youtube_music_api_request_types.h"
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
      const CreateRequestSenderCallback& create_request_sender_callback,
      std::unique_ptr<::ash::RequestSigner> request_signer);
  YouTubeMusicClient(const YouTubeMusicClient&) = delete;
  YouTubeMusicClient& operator=(const YouTubeMusicClient&) = delete;
  ~YouTubeMusicClient();

  // Invokes a request to the API server for music section data.
  void GetMusicSection(GetMusicSectionCallback callback);

  // Invokes a request to the API server for a specific playlist with id
  // `playlist_id`.
  void GetPlaylist(const std::string& playlist_id,
                   youtube_music::GetPlaylistCallback callback);

  // Invokes a request to the API server for preparing the playback queue.
  void PlaybackQueuePrepare(const std::string& playlist_id,
                            GetPlaybackContextCallback callback);

  // Invokes a request to the API server for requesting the next track in in the
  // playback queue.
  void PlaybackQueueNext(const std::string& playback_queue_id,
                         GetPlaybackContextCallback callback);

  // Invokes a request to report the playback to the API server.
  void ReportPlayback(const std::string& playback_reporting_token,
                      const PlaybackData& playback_data,
                      ReportPlaybackCallback callback);

 private:
  google_apis::RequestSender* GetRequestSender();

  // Triggered when music section data is fetched.
  void OnGetMusicSectionRequestDone(
      const base::Time& request_start_time,
      base::expected<
          std::unique_ptr<
              google_apis::youtube_music::TopLevelMusicRecommendations>,
          google_apis::youtube_music::ApiError> result);

  // Triggered when playlist with name `playlist_id` is fetched.
  void OnGetPlaylistRequestDone(
      const std::string& playlist_id,
      const base::Time& request_start_time,
      base::expected<std::unique_ptr<google_apis::youtube_music::Playlist>,
                     google_apis::youtube_music::ApiError> result);

  // Triggered when play context is fetched by preparing the playback queue.
  void OnPlaybackQueuePrepareRequestDone(
      const base::Time& request_start_time,
      base::expected<std::unique_ptr<google_apis::youtube_music::Queue>,
                     google_apis::youtube_music::ApiError> result);

  // Triggered when play context is fetched by requesting next in the playback
  // queue.
  void OnPlaybackQueueNextRequestDone(
      const base::Time& request_start_time,
      base::expected<
          std::unique_ptr<google_apis::youtube_music::QueueContainer>,
          google_apis::youtube_music::ApiError> result);

  // Triggered when report playback request is done.
  void OnReportPlaybackRequestDone(
      const base::Time& request_start_time,
      base::expected<
          std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult>,
          google_apis::youtube_music::ApiError> result);

  void OnRequestSigned(
      google_apis::RequestSender* request_sender,
      std::unique_ptr<google_apis::youtube_music::SignedRequest> signed_request,
      const std::vector<std::string>& headers);

  // Callback passed in at initialization time for creating request sender.
  CreateRequestSenderCallback create_request_sender_callback_;

  // Callback that runs when music section data is fetched.
  GetMusicSectionCallback music_section_callback_;

  // Callback that runs when playlists are fetched by ID.
  base::flat_map<std::string, GetPlaylistCallback> playlist_callback_map_;

  // Callback that runs when playback context data is fetched by preparing the
  // playback queue.
  GetPlaybackContextCallback playback_context_prepare_callback_;

  // Callback that runs when playback context data is fetched by requesting next
  // in the playback queue.
  GetPlaybackContextCallback playback_context_next_callback_;

  // Callback that runs when playback report is done.
  ReportPlaybackCallback report_playback_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // Helper class that signs POST requests with a client certificate.
  std::unique_ptr<::ash::RequestSigner> request_signer_;

  base::WeakPtrFactory<YouTubeMusicClient> weak_factory_{this};
};

}  // namespace ash::youtube_music

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_CLIENT_H_
