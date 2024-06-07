// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/youtube_music/youtube_music_client.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "ash/system/focus_mode/youtube_music/youtube_music_types.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/youtube_music/youtube_music_api_request_types.h"
#include "google_apis/youtube_music/youtube_music_api_requests.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"

namespace ash::youtube_music {

namespace {

// Traffic annotation tag for system admins and regulators.
// TODO(yongshun): Figure out if we need to add a policy.
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
          policy_exception_justification:
            "Experimental feature disabled by default. Policy not yet "
            "implemented."
        }
    )");

// Returns the pointer of the most appropriate image to use. When there are
// images that meet the minimal width and height requirements, it uses the
// smallest image to speed things up; otherwise it uses the largest image
// available.
google_apis::youtube_music::Image* FindAppropriateImage(
    std::vector<std::unique_ptr<google_apis::youtube_music::Image>>* images) {
  if (!images || images->empty()) {
    return nullptr;
  }

  auto bigger_in_size =
      [](const std::unique_ptr<google_apis::youtube_music::Image>& img1,
         const std::unique_ptr<google_apis::youtube_music::Image>& img2) {
        if (!img1) {
          return false;
        }
        if (!img2) {
          return true;
        }
        return img1->width() * img1->height() > img2->width() * img2->height();
      };
  auto qualified =
      [](const std::unique_ptr<google_apis::youtube_music::Image>& img) {
        return img && img->width() >= kImageMinimalWidth &&
               img->height() >= kImageMinimalHeight;
      };
  size_t smallest_qualified_index = images->size();
  for (size_t i = 0; i < images->size(); i++) {
    if (qualified(images->at(i)) &&
        (smallest_qualified_index == images->size() ||
         bigger_in_size(images->at(smallest_qualified_index), images->at(i)))) {
      smallest_qualified_index = i;
    }
  }

  return smallest_qualified_index < images->size()
             ? images->at(smallest_qualified_index).get()
             : std::max_element(images->begin(), images->end(), bigger_in_size)
                   ->get();
}

// Gets `Image` from API image. Please note, `api_iamge` could be null.
// TODO(yongshun): Consider add a default image.
Image FromApiImage(const google_apis::youtube_music::Image* api_iamge) {
  auto image = Image(0, 0, GURL());
  if (api_iamge) {
    image.width = api_iamge->width();
    image.height = api_iamge->height();
    image.url = api_iamge->url();
  }
  return image;
}

// Gets a vector of `Playlist` from `top_level_music_recommendations`.
std::optional<std::vector<Playlist>>
GetPlaylistsFromTopLevelMusicRecommendations(
    google_apis::youtube_music::TopLevelMusicRecommendations*
        top_level_music_recommendations) {
  if (!top_level_music_recommendations) {
    return std::nullopt;
  }

  std::vector<Playlist> playlists;
  for (auto& top_level_recommendation :
       *top_level_music_recommendations
            ->mutable_top_level_music_recommendations()) {
    for (auto& music_recommendation : *top_level_recommendation->music_section()
                                           .mutable_music_recommendations()) {
      auto& playlist = music_recommendation->playlist();
      playlists.emplace_back(
          playlist.name(), playlist.title(), playlist.owner().title(),
          FromApiImage(FindAppropriateImage(playlist.mutable_images())));
    }
  }
  return playlists;
}

// Gets `Playlist` from API playlist.
std::optional<Playlist> GetPlaylistFromApiPlaylist(
    google_apis::youtube_music::Playlist* playlist) {
  if (!playlist) {
    return std::nullopt;
  }

  return Playlist(
      playlist->name(), playlist->title(), playlist->owner().title(),
      FromApiImage(FindAppropriateImage(playlist->mutable_images())));
}

// Gets `PlaybackContext` from `queue`.
std::optional<PlaybackContext> GetPlaybackContextFromPlaybackQueue(
    google_apis::youtube_music::Queue* queue) {
  if (!queue) {
    return std::nullopt;
  }

  auto& playback_context = queue->playback_context();
  auto& track = playback_context.queue_item().track();
  // TODO(yongshun): Consider to add retry when there is no stream in the
  // response.
  GURL stream_url = GURL();
  if (auto* mutable_streams =
          playback_context.playback_manifest().mutable_streams();
      !mutable_streams->empty()) {
    stream_url = mutable_streams->begin()->get()->url();
  }
  return PlaybackContext(
      track.name(), track.title(), track.explicit_type(),
      FromApiImage(FindAppropriateImage(track.mutable_images())), stream_url,
      queue->name());
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
           GetPlaylistsFromTopLevelMusicRecommendations(result.value().get()));
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
           GetPlaybackContextFromPlaybackQueue(result.value().get()));
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
           GetPlaybackContextFromPlaybackQueue(&result.value()->queue()));
}

}  // namespace ash::youtube_music
