// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include <memory>

#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {

namespace {

constexpr int kPlaylistNum = 4;

struct DummyPlaylistData {
  const char* id;
  const char* title;
  const char* uri;
};

// The below uri placeholder data got from
// https://developers.google.com/youtube/mediaconnect/guides/recommendations
constexpr DummyPlaylistData kPlaceholderSoundscapeData[] = {
    {.id = "playlists/soundscape0",
     .title = "Nature",
     .uri =
         "https://music.youtube.com/image/"
         "mixart?r=ENgEGNgEMiMICxABGg0vZy8xMWJ3ZjZzbGdzGgovbS8wNDlnNXJkIgJlbg"},
    {.id = "playlists/soundscape1",
     .title = "Ambiance",
     .uri =
         "https://music.youtube.com/image/"
         "mixart?r=ENgEGNgEMiMICxABGg0vZy8xMWJ3ZjZzbGdzGgovbS8wNDlnNXJkIgJlbg"},
    {.id = "playlists/soundscape2",
     .title = "Classical",
     .uri =
         "https://music.youtube.com/image/"
         "mixart?r=ENgEGNgEMiMICxABGg0vZy8xMWJ3ZjZzbGdzGgovbS8wNDlnNXJkIgJlbg"},
    {.id = "playlists/soundscape3",
     .title = "Zen",
     .uri =
         "https://music.youtube.com/image/"
         "mixart?r=ENgEGNgEMiMICxABGg0vZy8xMWJ3ZjZzbGdzGgovbS8wNDlnNXJkIgJlbg"},
};

constexpr DummyPlaylistData kPlaceholderYoutubeMusicData[] = {
    {.id = "playlists/youtubemusic0",
     .title = "Chill R&B",
     .uri = "https://music.youtube.com/image/"
            "mixart?r="
            "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4"},
    {.id = "playlists/youtubemusic1",
     .title = "Unwind Test Long Name",
     .uri = "https://music.youtube.com/image/"
            "mixart?r="
            "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4"},
    {.id = "playlists/youtubemusic2",
     .title = "Velvet Voices",
     .uri = "https://music.youtube.com/image/"
            "mixart?r="
            "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4"},
    {.id = "playlists/youtubemusic3",
     .title = "Lofi Loft",
     .uri = "https://music.youtube.com/image/"
            "mixart?r="
            "ENgEGNgEMiQICxACGg0vZy8xMWJ3ZjZzbGdzGgsvbS8wMTBqN3JsciICZW4"},
};

// TODO(b/328121041): Update the field for `policy_exception_justification`
// after we added a policy and keep the `user_data` up-to-date.
constexpr net::NetworkTrafficAnnotationTag kFocusModeSoundsThumbnailTag =
    net::DefineNetworkTrafficAnnotation("focus_mode_sounds_image_downloader",
                                        R"(
        semantics {
          sender: "Focus Mode"
          description:
            "Download YouTube Music playlist thumbnails which will be shown "
            "on the focus mode panel."
          trigger: "User opens a panel in Focus Mode."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "hongyulong@google.com"
            }
            contacts {
              email: "chromeos-wms@google.com"
            }
          }
          last_reviewed: "2024-03-15"
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by user."
         policy_exception_justification:
           "Experimental feature disabled by default. Policy not yet "
           "implemented."
        })");

void DownloadImageFromUrl(const std::string& url,
                          ImageDownloader::DownloadCallback callback) {
  CHECK(!url.empty());

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  DCHECK(active_user_session);

  ImageDownloader::Get()->Download(GURL(url), kFocusModeSoundsThumbnailTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

}  // namespace

FocusModeSoundsController::FocusModeSoundsController() {
  soundscape_playlists_.reserve(kPlaylistNum);
  youtube_music_playlists_.reserve(kPlaylistNum);
}

FocusModeSoundsController::~FocusModeSoundsController() = default;

void FocusModeSoundsController::DownloadPlaylistsForType(
    const bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback) {
  // During shutdown, `ImageDownloader` may not exist here.
  if (!ImageDownloader::Get()) {
    return;
  }

  // TODO(b/321071604): Currently, when opening the focus panel, we will clean
  // up all saved data and then download all playlists. In the future, we can
  // keep this cached and update if there are new playlists.
  auto barrier_callback = base::BarrierCallback<std::unique_ptr<Playlist>>(
      /*num_callbacks=*/kPlaylistNum, /*done_callback=*/base::BindOnce(
          &FocusModeSoundsController::OnAllThumbnailsDownloaded,
          weak_factory_.GetWeakPtr(), is_soundscape_type,
          std::move(update_sounds_view_callback)));

  const auto& data = is_soundscape_type ? kPlaceholderSoundscapeData
                                        : kPlaceholderYoutubeMusicData;
  for (const auto& item : data) {
    DownloadImageFromUrl(
        item.uri,
        base::BindOnce(&FocusModeSoundsController::OnOneThumbnailDownloaded,
                       weak_factory_.GetWeakPtr(), barrier_callback, item.id,
                       item.title));
  }
}

void FocusModeSoundsController::OnOneThumbnailDownloaded(
    base::OnceCallback<void(std::unique_ptr<Playlist>)> barrier_callback,
    std::string id,
    std::string title,
    const gfx::ImageSkia& thumbnail) {
  if (thumbnail.isNull()) {
    return;
  }

  std::move(barrier_callback)
      .Run(std::make_unique<Playlist>(id, title, thumbnail));
}

void FocusModeSoundsController::OnAllThumbnailsDownloaded(
    bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback,
    std::vector<std::unique_ptr<Playlist>> playlists) {
  if (is_soundscape_type) {
    soundscape_playlists_.swap(playlists);
  } else {
    youtube_music_playlists_.swap(playlists);
  }

  // Only trigger the observer function when all the thumbnails are finished
  // downloading.
  // TODO(b/321071604): We may need to update this once caching is implemented.
  std::move(update_sounds_view_callback).Run(is_soundscape_type);
}

}  // namespace ash
