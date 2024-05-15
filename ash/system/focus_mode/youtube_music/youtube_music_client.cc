// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/youtube_music/youtube_music_client.h"

#include "base/functional/callback_helpers.h"
#include "google_apis/gaia/gaia_constants.h"

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

}  // namespace

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

}  // namespace ash::youtube_music
