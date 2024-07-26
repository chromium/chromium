// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_client.h"

#include <memory>
#include <optional>

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_util.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/youtube_music/youtube_music_api_request_types.h"
#include "google_apis/youtube_music/youtube_music_api_requests.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"
#include "net/base/network_change_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::youtube_music {

namespace {

// Traffic annotation tag for system admins and regulators.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("youtube_music_integration",
                                        R"(
        semantics {
          sender: "Chrome YouTube Music delegate"
          description: "Provides ChromeOS users access to their YouTube Music "
                       "contents without opening the app or website."
          trigger: "User opens a panel in Focus Mode."
          data: "The request is authenticated with an OAuth2 access token "
                "identifying the Google account."
          internal {
            contacts {
              email: "yongshun@google.com"
            }
            contacts {
              email: "chromeos-wms@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-05-08"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
            FocusModeSoundsEnabled {
              FocusModeSoundsEnabled: "focus-sounds"
            }
          }
        }
    )");

google_apis::youtube_music::ReportPlaybackRequestPayload::PlaybackState
GetPayloadPlaybackState(const PlaybackState player_state) {
  switch (player_state) {
    case PlaybackState::kPlaying:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          PlaybackState::kPlaying;
    case PlaybackState::kPaused:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          PlaybackState::kPaused;
    case PlaybackState::kSwitchedToNext:
    case PlaybackState::kEnded:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          PlaybackState::kCompleted;
    default:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          PlaybackState::kUnspecified;
  }
}

// Returns connection type to report to the YouTube Music API server.
// Definitions can be found at:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/reports/playback#connectiontype
google_apis::youtube_music::ReportPlaybackRequestPayload::ConnectionType
GetNetworkConnectionType() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kUnspecified;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kWired;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI:
      if (net::NetworkChangeNotifier::GetConnectionCost() ==
          net::NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED) {
        return google_apis::youtube_music::ReportPlaybackRequestPayload::
            ConnectionType::kWifiMetered;
      } else {
        return google_apis::youtube_music::ReportPlaybackRequestPayload::
            ConnectionType::kWifi;
      }

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kCellular2g;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kCellular3g;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kCellular4g;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kNone;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH:
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kDisco;

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G:
      // TODO(yongshun): ChromeOS does not detect 5G sub types yet (standalone
      // cellular connection or non-standalone cellular connection). Update to
      // use `kCellular5gSa` or `kCellular5gNsa` once it can differentiate.
      return google_apis::youtube_music::ReportPlaybackRequestPayload::
          ConnectionType::kActiveUncategorized;
  }
}

std::unique_ptr<google_apis::youtube_music::ReportPlaybackRequestPayload>
CreateReportPlaybackRequestPayload(const std::string& playback_reporting_token,
                                   const PlaybackData& playback_data) {
  base::Time current_time = base::Time::Now();
  std::optional<google_apis::youtube_music::ReportPlaybackRequestPayload::
                    WatchTimeSegment>
      watch_time_segment = std::nullopt;
  if (!playback_data.initial_playback &&
      playback_data.media_start.has_value() &&
      playback_data.media_end.has_value()) {
    watch_time_segment = google_apis::youtube_music::
        ReportPlaybackRequestPayload::WatchTimeSegment(
            base::Seconds(playback_data.media_start.value()),
            base::Seconds(playback_data.media_end.value()), current_time);
  }
  google_apis::youtube_music::ReportPlaybackRequestPayload::Params param(
      playback_reporting_token, current_time, base::TimeDelta(),
      base::TimeDelta(), GetNetworkConnectionType(),
      GetPayloadPlaybackState(playback_data.state), watch_time_segment);
  return std::make_unique<
      google_apis::youtube_music::ReportPlaybackRequestPayload>(param);
}

}  // namespace

YouTubeMusicClient::YouTubeMusicClient(
    const CreateRequestSenderCallback& create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

YouTubeMusicClient::~YouTubeMusicClient() = default;

void YouTubeMusicClient::GetMusicSection(GetMusicSectionCallback callback) {
  CHECK(callback);
  music_section_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::GetMusicSectionRequest>(
          request_sender,
          base::BindOnce(&YouTubeMusicClient::OnGetMusicSectionRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now())));
}

void YouTubeMusicClient::GetPlaylist(
    const std::string& playlist_id,
    youtube_music::GetPlaylistCallback callback) {
  CHECK(callback);
  playlist_callback_map_[playlist_id] = std::move(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::GetPlaylistRequest>(
          request_sender, playlist_id,
          base::BindOnce(&YouTubeMusicClient::OnGetPlaylistRequestDone,
                         weak_factory_.GetWeakPtr(), playlist_id,
                         base::Time::Now())));
}

void YouTubeMusicClient::PlaybackQueuePrepare(
    const std::string& playlist_id,
    GetPlaybackContextCallback callback) {
  CHECK(callback);
  playback_context_prepare_callback_ = std::move(callback);

  auto request_payload =
      google_apis::youtube_music::PlaybackQueuePrepareRequestPayload(
          playlist_id,
          google_apis::youtube_music::PlaybackQueuePrepareRequestPayload::
              ExplicitFilter::kBestEffort,
          google_apis::youtube_music::PlaybackQueuePrepareRequestPayload::
              ShuffleMode::kOn);
  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::PlaybackQueuePrepareRequest>(
          request_sender, request_payload,
          base::BindOnce(&YouTubeMusicClient::OnPlaybackQueuePrepareRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now())));
}

void YouTubeMusicClient::PlaybackQueueNext(
    const std::string& playback_queue_id,
    GetPlaybackContextCallback callback) {
  CHECK(callback);
  playback_context_next_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::PlaybackQueueNextRequest>(
          request_sender,
          base::BindOnce(&YouTubeMusicClient::OnPlaybackQueueNextRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now()),
          playback_queue_id));
}

void YouTubeMusicClient::ReportPlayback(
    const std::string& playback_reporting_token,
    const PlaybackData& playback_data,
    ReportPlaybackCallback callback) {
  CHECK(callback);
  report_playback_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::ReportPlaybackRequest>(
          request_sender,
          CreateReportPlaybackRequestPayload(playback_reporting_token,
                                             playback_data),
          base::BindOnce(&YouTubeMusicClient::OnReportPlaybackRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now())));
}

google_apis::RequestSender* YouTubeMusicClient::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ =
        std::move(create_request_sender_callback_)
            .Run({GaiaConstants::kYouTubeMusicOAuth2Scope}, kTrafficAnnotation);
    create_request_sender_callback_ = base::NullCallback();
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

void YouTubeMusicClient::OnGetMusicSectionRequestDone(
    const base::Time& request_start_time,
    base::expected<
        std::unique_ptr<
            google_apis::youtube_music::TopLevelMusicRecommendations>,
        google_apis::ApiErrorCode> result) {
  if (!music_section_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(music_section_callback_).Run(result.error(), std::nullopt);
    return;
  }

  std::move(music_section_callback_)
      .Run(google_apis::HTTP_SUCCESS,
           GetPlaylistsFromApiTopLevelMusicRecommendations(
               result.value().get()));
}

void YouTubeMusicClient::OnGetPlaylistRequestDone(
    const std::string& playlist_id,
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::Playlist>,
                   google_apis::ApiErrorCode> result) {
  if (playlist_callback_map_.find(playlist_id) ==
      playlist_callback_map_.end()) {
    return;
  }

  GetPlaylistCallback playlist_callback =
      std::move(playlist_callback_map_[playlist_id]);
  playlist_callback_map_.erase(playlist_id);

  if (!playlist_callback) {
    return;
  }

  if (!result.has_value()) {
    std::move(playlist_callback).Run(result.error(), std::nullopt);
    return;
  }

  std::move(playlist_callback)
      .Run(google_apis::HTTP_SUCCESS,
           GetPlaylistFromApiPlaylist(result.value().get()));
}

void YouTubeMusicClient::OnPlaybackQueuePrepareRequestDone(
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::Queue>,
                   google_apis::ApiErrorCode> result) {
  if (!playback_context_prepare_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(playback_context_prepare_callback_)
        .Run(result.error(), std::nullopt);
    return;
  }

  if (!result.value()) {
    std::move(playback_context_prepare_callback_)
        .Run(google_apis::ApiErrorCode::HTTP_SUCCESS, std::nullopt);
    return;
  }

  std::move(playback_context_prepare_callback_)
      .Run(google_apis::HTTP_SUCCESS,
           GetPlaybackContextFromApiQueue(result.value().get()));
}

void YouTubeMusicClient::OnPlaybackQueueNextRequestDone(
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::QueueContainer>,
                   google_apis::ApiErrorCode> result) {
  if (!playback_context_next_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(playback_context_next_callback_)
        .Run(result.error(), std::nullopt);
    return;
  }

  if (!result.value()) {
    std::move(playback_context_next_callback_)
        .Run(google_apis::ApiErrorCode::HTTP_SUCCESS, std::nullopt);
    return;
  }

  std::move(playback_context_next_callback_)
      .Run(google_apis::HTTP_SUCCESS,
           GetPlaybackContextFromApiQueue(&result.value()->queue()));
}

void YouTubeMusicClient::OnReportPlaybackRequestDone(
    const base::Time& request_start_time,
    base::expected<
        std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult>,
        google_apis::ApiErrorCode> result) {
  if (!report_playback_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(report_playback_callback_).Run(result.error(), std::nullopt);
    return;
  }

  if (!result.value()) {
    std::move(report_playback_callback_)
        .Run(google_apis::ApiErrorCode::HTTP_SUCCESS, std::nullopt);
    return;
  }

  std::move(report_playback_callback_)
      .Run(google_apis::HTTP_SUCCESS,
           result.value()->playback_reporting_token());
}

}  // namespace ash::youtube_music
