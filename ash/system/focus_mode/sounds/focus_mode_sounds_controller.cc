// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"
#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"
#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
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
}

namespace {

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

// Re-order `playlists` according to the order of `data`.
void ReorderPlaylists(
    const std::vector<FocusModeSoundsDelegate::Playlist>& data,
    base::OnceCallback<
        void(std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>)>
        sorted_playlists_callback,
    std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>
        unsorted_playlists) {
  std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>
      sorted_playlists;

  // Create `sorted_playlists` to match the given order.
  for (const auto& item : data) {
    auto iter = std::find_if(
        unsorted_playlists.begin(), unsorted_playlists.end(),
        [item](const std::unique_ptr<FocusModeSoundsController::Playlist>&
                   playlist) {
          return playlist && playlist->playlist_id == item.id;
        });
    if (iter == unsorted_playlists.end()) {
      continue;
    }

    sorted_playlists.push_back(std::move(*iter));
  }

  std::move(sorted_playlists_callback).Run(std::move(sorted_playlists));
}

// In response to receiving the playlists, start downloading the playlist
// thumbnails.
void DispatchRequests(
    base::OnceCallback<
        void(std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>)>
        sorted_playlists_callback,
    const std::vector<FocusModeSoundsDelegate::Playlist>& data) {
  if (data.empty()) {
    LOG(WARNING) << "Retrieving Playlist data failed.";
    std::move(sorted_playlists_callback).Run({});
    return;
  }

  CHECK_EQ(static_cast<int>(data.size()), kPlaylistNum);

  // TODO(b/340304748): Currently, when opening the focus panel, we will clean
  // up all saved data and then download all playlists. In the future, we can
  // keep this cached and update if there are new playlists.
  using BarrierReturn = std::unique_ptr<FocusModeSoundsController::Playlist>;
  auto barrier_callback = base::BarrierCallback<BarrierReturn>(
      /*num_callbacks=*/kPlaylistNum,
      /*done_callback=*/base::BindOnce(&ReorderPlaylists, data,
                                       std::move(sorted_playlists_callback)));

  for (const auto& item : data) {
    FocusModeSoundsController::DownloadTrackThumbnail(
        item.thumbnail_url,
        base::BindOnce(&OnOneThumbnailDownloaded, barrier_callback, item.id,
                       item.title));
  }
}

// In response to receiving the track, start playing the track.
void OnTrackFetched(
    FocusModeSoundsController::GetNextTrackCallback callback,
    const std::optional<FocusModeSoundsDelegate::Track>& track) {
  if (!track) {
    // TODO(b/343961303): Potentially retry the request.
    LOG(WARNING) << "Retrieving track failed";
  }

  std::move(callback).Run(track);
}

// Parses the ash.focus_mode.sounds_enabled pref and returns a set of the
// `SoundType`s that should be enabled.
base::flat_set<focus_mode_util::SoundType> ReadSoundSectionPolicy(
    const PrefService* pref_service) {
  CHECK(pref_service);
  const std::string& enabled_sections_pref =
      pref_service->GetString(prefs::kFocusModeSoundsEnabled);

  if (enabled_sections_pref == focus_mode_util::kFocusModeSoundsEnabled) {
    return {focus_mode_util::SoundType::kSoundscape,
            focus_mode_util::SoundType::kYouTubeMusic};
  } else if (enabled_sections_pref == focus_mode_util::kFocusSoundsOnly) {
    return {focus_mode_util::SoundType::kSoundscape};
  } else if (enabled_sections_pref ==
             focus_mode_util::kFocusModeSoundsDisabled) {
    return {};
  }

  // Unrecognized value. It's likely a new restriction so disable everything.
  return {};
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
    : soundscape_delegate_(FocusModeSoundscapeDelegate::Create("en-US")),
      youtube_music_delegate_(
          std::make_unique<FocusModeYouTubeMusicDelegate>()) {
  // TODO(b/341176182): Plumb the locale here and replace the default
  // locale.
  soundscape_playlists_.reserve(kPlaylistNum);
  youtube_music_playlists_.reserve(kPlaylistNum);
}

FocusModeSoundsController::~FocusModeSoundsController() = default;

// static
void FocusModeSoundsController::DownloadTrackThumbnail(
    const GURL& url,
    ImageDownloader::DownloadCallback callback) {
  CHECK(!url.is_empty());

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  CHECK(active_user_session);

  ImageDownloader::Get()->Download(url, kFocusModeSoundsThumbnailTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

void FocusModeSoundsController::GetNextTrack(GetNextTrackCallback callback) {
  if (selected_playlist_.type == focus_mode_util::SoundType::kNone ||
      selected_playlist_.id.empty()) {
    LOG(WARNING) << "No selected playlist";
    std::move(callback).Run(std::nullopt);
    return;
  }

  FocusModeSoundsDelegate* delegate;
  if (selected_playlist_.type == focus_mode_util::SoundType::kSoundscape) {
    delegate = soundscape_delegate_.get();
  } else if (selected_playlist_.type ==
             focus_mode_util::SoundType::kYouTubeMusic) {
    delegate = youtube_music_delegate_.get();
  } else {
    LOG(ERROR) << "Unrecognized playlist type";
    std::move(callback).Run(std::nullopt);
    return;
  }

  delegate->GetNextTrack(selected_playlist_.id,
                         base::BindOnce(&OnTrackFetched, std::move(callback)));
}

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

  auto sorted_playlists_callback =
      base::BindOnce(&FocusModeSoundsController::OnAllThumbnailsDownloaded,
                     weak_factory_.GetWeakPtr(), is_soundscape_type,
                     std::move(update_sounds_view_callback));

  if (is_soundscape_type) {
    soundscape_delegate_->GetPlaylists(base::BindOnce(
        &DispatchRequests, std::move(sorted_playlists_callback)));
  } else {
    youtube_music_delegate_->GetPlaylists(base::BindOnce(
        &DispatchRequests, std::move(sorted_playlists_callback)));
  }
}

void FocusModeSoundsController::UpdateFromUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    return;
  }

  const auto& dict = active_user_prefs->GetDict(prefs::kFocusModeSoundSection);

  // If the user didn't select any playlist before, we should show the
  // `Soundscape` sound section as default behavior.
  if (dict.empty()) {
    sound_type_ = focus_mode_util::SoundType::kSoundscape;
  } else {
    sound_type_ = static_cast<focus_mode_util::SoundType>(
        dict.FindInt(focus_mode_util::kSoundTypeKey).value());
  }

  base::flat_set<focus_mode_util::SoundType> enabled_sections =
      ReadSoundSectionPolicy(active_user_prefs);
  // TODO(b/328121041): Push section information into the views.
}

void FocusModeSoundsController::SetYouTubeMusicFailureCallback(
    base::RepeatingClosure callback) {
  CHECK(callback);
  youtube_music_delegate_->SetFailureCallback(std::move(callback));
}

void FocusModeSoundsController::SaveUserPref() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    base::Value::Dict dict;
    dict.Set(focus_mode_util::kSoundTypeKey, static_cast<int>(sound_type_));
    dict.Set(focus_mode_util::kPlaylistIdKey, selected_playlist_.id);
    active_user_prefs->SetDict(prefs::kFocusModeSoundSection, std::move(dict));
  }
}

void FocusModeSoundsController::ResetSelectedPlaylist() {
  // TODO: Stop the music for current selected playlist.
  selected_playlist_ = {};

  // We still want to keep the user pref for sound section after deselecting the
  // selected playlist.
  SaveUserPref();
  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
}

void FocusModeSoundsController::SelectPlaylist(
    const SelectedPlaylist& playlist_data) {
  selected_playlist_ = playlist_data;

  // TODO(b/337063849): Update the sound state when the media stream
  // actually starts playing.
  selected_playlist_.state = focus_mode_util::SoundState::kSelected;
  sound_type_ = selected_playlist_.type;

  // Reserve a place for the last selected playlist for future use.
  if (sound_type_ == focus_mode_util::SoundType::kYouTubeMusic) {
    youtube_music_delegate_->ReservePlaylistForGetPlaylists(
        selected_playlist_.id);
  }

  SaveUserPref();
  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
}

void FocusModeSoundsController::OnAllThumbnailsDownloaded(
    bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback,
    std::vector<std::unique_ptr<Playlist>> sorted_playlists) {
  if (is_soundscape_type) {
    soundscape_playlists_.swap(sorted_playlists);
  } else {
    youtube_music_playlists_.swap(sorted_playlists);
  }

  // Only trigger the observer function when all the thumbnails are finished
  // downloading.
  // TODO(b/321071604): We may need to update this once caching is implemented.
  std::move(update_sounds_view_callback).Run(is_soundscape_type);
}

}  // namespace ash
