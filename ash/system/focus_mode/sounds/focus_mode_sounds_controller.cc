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
#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"
#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"
#include "ash/system/focus_mode/sounds/playlist_view.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr int kPlaylistNum = 4;

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

void DownloadImageFromUrl(const GURL& url,
                          ImageDownloader::DownloadCallback callback) {
  CHECK(!url.is_empty());

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  DCHECK(active_user_session);

  ImageDownloader::Get()->Download(url, kFocusModeSoundsThumbnailTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

// Invoked upon completion of the `thumbnail` download. `thumbnail` can be a
// null image if the download attempt from the url failed.
void OnOneThumbnailDownloaded(
    base::OnceCallback<void(
        std::unique_ptr<FocusModeSoundsController::Playlist>)> barrier_callback,
    std::string id,
    std::string title,
    const gfx::ImageSkia& thumbnail) {
  std::move(barrier_callback)
      .Run(std::make_unique<FocusModeSoundsController::Playlist>(id, title,
                                                                 thumbnail));
}

// In response to receiving the playlists, start downloading the playlist
// thumbnails.
void DispatchRequests(
    base::OnceCallback<
        void(std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>)>
        done_callback,
    const std::vector<FocusModeSoundsDelegate::Playlist>& data) {
  CHECK_EQ(data.size(), 4u);

  // TODO(b/340304748): Currently, when opening the focus panel, we will clean
  // up all saved data and then download all playlists. In the future, we can
  // keep this cached and update if there are new playlists.
  using BarrierReturn = std::unique_ptr<FocusModeSoundsController::Playlist>;
  auto barrier_callback = base::BarrierCallback<BarrierReturn>(
      /*num_callbacks=*/kPlaylistNum, std::move(done_callback));

  for (const auto& item : data) {
    DownloadImageFromUrl(item.thumbnail_url,
                         base::BindOnce(&OnOneThumbnailDownloaded,
                                        barrier_callback, item.id, item.title));
  }
}

}  // namespace

FocusModeSoundsController::SelectedPlaylist::SelectedPlaylist() = default;

FocusModeSoundsController::SelectedPlaylist::SelectedPlaylist(
    const SelectedPlaylist&) = default;

FocusModeSoundsController::SelectedPlaylist&
FocusModeSoundsController::SelectedPlaylist::operator=(
    const SelectedPlaylist& other) = default;

FocusModeSoundsController::SelectedPlaylist::~SelectedPlaylist() = default;

FocusModeSoundsController::FocusModeSoundsController()
    : soundscape_delegate_(std::make_unique<FocusModeSoundscapeDelegate>()),
      youtube_music_delegate_(
          std::make_unique<FocusModeYouTubeMusicDelegate>()) {
  soundscape_playlists_.reserve(kPlaylistNum);
  youtube_music_playlists_.reserve(kPlaylistNum);
}

FocusModeSoundsController::~FocusModeSoundsController() = default;

void FocusModeSoundsController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeSoundsController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeSoundsController::TogglePlaylist(
    const SelectedPlaylist& playlist_data) {
  if (playlist_data.state != focus_mode_util::SoundState::kNone) {
    // When the user toggles a selected playlist, we will deselect it.
    ResetSelectedPlaylist();
  } else {
    SelectPlaylist(playlist_data);
  }
}

void FocusModeSoundsController::DownloadPlaylistsForType(
    const bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback) {
  // During shutdown, `ImageDownloader` may not exist here.
  if (!ImageDownloader::Get()) {
    return;
  }

  auto done_callback =
      base::BindOnce(&FocusModeSoundsController::OnAllThumbnailsDownloaded,
                     weak_factory_.GetWeakPtr(), is_soundscape_type,
                     std::move(update_sounds_view_callback));

  if (is_soundscape_type) {
    soundscape_delegate_->GetPlaylists(
        base::BindOnce(&DispatchRequests, std::move(done_callback)));
  } else {
    youtube_music_delegate_->GetPlaylists(
        base::BindOnce(&DispatchRequests, std::move(done_callback)));
  }
}

void FocusModeSoundsController::ResetSelectedPlaylist() {
  // TODO: Stop the music for current selected playlist.
  selected_playlist_ = {};
  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
}

void FocusModeSoundsController::SelectPlaylist(
    const SelectedPlaylist& playlist_data) {
  selected_playlist_ = playlist_data;

  // TODO: If in an active focus session and toggling on a playlist, we should
  // trigger the player to start playing and set the state as `kPlaying`
  // instead.
  selected_playlist_.state = focus_mode_util::SoundState::kSelected;

  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
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
