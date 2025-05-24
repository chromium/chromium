// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_client.h"

#include <memory>
#include <optional>

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_util.h"
#include "base/containers/span.h"
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

// Runs `callback` with `PARSE_ERROR` for payloads that are unexpectedly null.
// `callback` is in an unspecified state after this runs.
template <class T>
void ReportParseError(T&& callback) {
  google_apis::youtube_music::ApiError api_error;
  api_error.error_code = google_apis::PARSE_ERROR;
  std::move(callback).Run(base::unexpected(api_error));
}

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
  std::vector<google_apis::youtube_music::ReportPlaybackRequestPayload::
                  WatchTimeSegment>
      watch_time_segments;
  watch_time_segments.reserve(playback_data.media_segments.size());
  for (const MediaSegment& media_segment : playback_data.media_segments) {
    watch_time_segments.emplace_back(
        google_apis::youtube_music::ReportPlaybackRequestPayload::
            WatchTimeSegment(base::Seconds(media_segment.media_start),
                             base::Seconds(media_segment.media_end),
                             media_segment.client_start_time));
  }
  google_apis::youtube_music::ReportPlaybackRequestPayload::Params param(
      playback_data.initial_playback, playback_reporting_token,
      playback_data.client_current_time,
      base::Seconds(playback_data.playback_start_offset),
      base::Seconds(playback_data.media_time_current),
      GetNetworkConnectionType(), GetPayloadPlaybackState(playback_data.state),
      watch_time_segments);
  return std::make_unique<
      google_apis::youtube_music::ReportPlaybackRequestPayload>(param);
}

}  // namespace

YouTubeMusicClient::YouTubeMusicClient(
    const CreateRequestSenderCallback& create_request_sender_callback,
    std::unique_ptr<RequestSigner> request_signer)
    : create_request_sender_callback_(create_request_sender_callback),
      request_signer_(std::move(request_signer)) {}

YouTubeMusicClient::~YouTubeMusicClient() = default;

void YouTubeMusicClient::GetMusicSection(GetMusicSectionCallback callback) {
  CHECK(callback);
  music_section_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<google_apis::youtube_music::GetMusicSectionRequest>(
          request_sender, request_signer_->DeviceInfoHeader(),
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
          request_sender, request_signer_->DeviceInfoHeader(), playlist_id,
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

  std::string payload = request_payload.ToJson();

  auto* const request_sender = GetRequestSender();
  google_apis::youtube_music::PlaybackQueuePrepareRequest::Callback
      request_callback =
          base::BindOnce(&YouTubeMusicClient::OnPlaybackQueuePrepareRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now());
  auto request =
      std::make_unique<google_apis::youtube_music::PlaybackQueuePrepareRequest>(
          request_sender, request_payload, std::move(request_callback));

  if (!request_signer_->GenerateHeaders(
          base::as_byte_span(payload),
          base::BindOnce(
              &YouTubeMusicClient::OnRequestSigned, weak_factory_.GetWeakPtr(),
              base::Unretained(request_sender), std::move(request)))) {
    LOG(WARNING) << "Cannot sign request";
    return;
  }
}

void YouTubeMusicClient::OnRequestSigned(
    google_apis::RequestSender* request_sender,
    std::unique_ptr<google_apis::youtube_music::SignedRequest> signed_request,
    const std::vector<std::string>& headers) {
  if (headers.empty()) {
    LOG(WARNING) << "Request signing failed. Cannot make API request.";
    return;
  }
  signed_request->SetSigningHeaders(std::vector<std::string>(headers));
  request_sender->StartRequestWithAuthRetry(std::move(signed_request));
}

void YouTubeMusicClient::PlaybackQueueNext(
    const std::string& playback_queue_id,
    GetPlaybackContextCallback callback) {
  CHECK(callback);
  playback_context_next_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  google_apis::youtube_music::PlaybackQueueNextRequest::Callback
      request_callback =
          base::BindOnce(&YouTubeMusicClient::OnPlaybackQueueNextRequestDone,
                         weak_factory_.GetWeakPtr(), base::Time::Now());

  google_apis::youtube_music::PlaybackQueueNextRequestPayload payload;
  std::string payload_string = payload.ToJson();

  auto request =
      std::make_unique<google_apis::youtube_music::PlaybackQueueNextRequest>(
          request_sender, payload, std::move(request_callback),
          playback_queue_id);

  if (!request_signer_->GenerateHeaders(
          base::as_byte_span(payload_string),
          base::BindOnce(
              &YouTubeMusicClient::OnRequestSigned, weak_factory_.GetWeakPtr(),
              base::Unretained(request_sender), std::move(request)))) {
    LOG(WARNING) << "Cannot sign request";
    return;
  }
}

void YouTubeMusicClient::ReportPlayback(
    const std::string& playback_reporting_token,
    const PlaybackData& playback_data,
    ReportPlaybackCallback callback) {
  CHECK(callback);
  report_playback_callback_ = std::move(callback);

  auto* const request_sender = GetRequestSender();
  auto payload = CreateReportPlaybackRequestPayload(playback_reporting_token,
                                                    playback_data);
  std::string json = payload->ToJson();
  auto request_callback =
      base::BindOnce(&YouTubeMusicClient::OnReportPlaybackRequestDone,
                     weak_factory_.GetWeakPtr(), base::Time::Now());
  auto request =
      std::make_unique<google_apis::youtube_music::ReportPlaybackRequest>(
          request_sender, std::move(payload), std::move(request_callback));
  if (!request_signer_->GenerateHeaders(
          base::as_byte_span(json),
          base::BindOnce(
              &YouTubeMusicClient::OnRequestSigned, weak_factory_.GetWeakPtr(),
              base::Unretained(request_sender), std::move(request)))) {
    LOG(WARNING) << "Cannot sign request";
    return;
  }
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
        google_apis::youtube_music::ApiError> result) {
  if (!music_section_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(music_section_callback_).Run(base::unexpected(result.error()));
    return;
  }

  if (!result.value()) {
    // This result is always expected to have contents.
    ReportParseError(std::move(music_section_callback_));
    return;
  }

  std::move(music_section_callback_)
      .Run(GetPlaylistsFromApiTopLevelMusicRecommendations(*result.value()));
}

void YouTubeMusicClient::OnGetPlaylistRequestDone(
    const std::string& playlist_id,
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::Playlist>,
                   google_apis::youtube_music::ApiError> result) {
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
    std::move(playlist_callback).Run(base::unexpected(result.error()));
    return;
  }

  if (!result.value()) {
    // This result is always expected to have contents.
    ReportParseError(std::move(playlist_callback));
    return;
  }

  std::move(playlist_callback).Run(GetPlaylistFromApiPlaylist(*result.value()));
}

void YouTubeMusicClient::OnPlaybackQueuePrepareRequestDone(
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::Queue>,
                   google_apis::youtube_music::ApiError> result) {
  if (!playback_context_prepare_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(playback_context_prepare_callback_)
        .Run(base::unexpected(result.error()));
    return;
  }

  if (!result.value()) {
    ReportParseError(std::move(playback_context_prepare_callback_));
    return;
  }

  std::move(playback_context_prepare_callback_)
      .Run(GetPlaybackContextFromApiQueue(*result.value()));
}

void YouTubeMusicClient::OnPlaybackQueueNextRequestDone(
    const base::Time& request_start_time,
    base::expected<std::unique_ptr<google_apis::youtube_music::QueueContainer>,
                   google_apis::youtube_music::ApiError> result) {
  if (!playback_context_next_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(playback_context_next_callback_)
        .Run(base::unexpected(result.error()));
    return;
  }

  if (!result.value()) {
    // This result is always expected to have contents.
    ReportParseError(std::move(playback_context_next_callback_));
    return;
  }

  std::move(playback_context_next_callback_)
      .Run(GetPlaybackContextFromApiQueue(result.value()->queue()));
}

void YouTubeMusicClient::OnReportPlaybackRequestDone(
    const base::Time& request_start_time,
    base::expected<
        std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult>,
        google_apis::youtube_music::ApiError> result) {
  if (!report_playback_callback_) {
    return;
  }

  if (!result.has_value()) {
    std::move(report_playback_callback_).Run(base::unexpected(result.error()));
    return;
  }

  if (!result.value()) {
    ReportParseError(std::move(report_playback_callback_));
    return;
  }

  std::move(report_playback_callback_)
      .Run(result.value()->playback_reporting_token());
}

}  // namespace ash::youtube_music
