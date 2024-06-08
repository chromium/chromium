// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_

#include <optional>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class FocusModeYouTubeMusicDelegate;

// This class is used to download images and record the info of playlists after
// getting the response data we need from Music API, which will be used to show
// on `FocusModeSoundsView`.
class ASH_EXPORT FocusModeSoundsController {
 public:
  using UpdateSoundsViewCallback = base::OnceCallback<void(bool)>;

  // The data used to display on the focus panel. It will include a playlist id,
  // a string of its title, and the downloaded thumbnail for the playlist cover
  // currently. We will add the stream info in future.
  struct Playlist {
    // Playlist identifier.
    std::string playlist_id;

    // Title of the playlist.
    std::string title;

    // Playlist cover downloaded through its image url.
    gfx::ImageSkia thumbnail;
  };

  struct SelectedPlaylist {
    SelectedPlaylist();
    SelectedPlaylist(const SelectedPlaylist& other);
    SelectedPlaylist& operator=(const SelectedPlaylist& other);
    ~SelectedPlaylist();

    bool empty() const { return id.empty(); }

    std::string id;
    std::string title;
    gfx::ImageSkia thumbnail;
    focus_mode_util::SoundType type = focus_mode_util::SoundType::kNone;
    focus_mode_util::SoundState state = focus_mode_util::SoundState::kNone;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a playlist is toggled by the user on the focus panel.
    virtual void OnSelectedPlaylistChanged() = 0;
  };

  FocusModeSoundsController();
  FocusModeSoundsController(const FocusModeSoundsController&) = delete;
  FocusModeSoundsController& operator=(const FocusModeSoundsController&) =
      delete;
  ~FocusModeSoundsController();

  // Download the artwork for a track. Exposed here so that native portion of
  // the focus mode web UI can download the artwork using the focus mode network
  // traffic annotation.
  static void DownloadTrackThumbnail(
      const GURL& url,
      ImageDownloader::DownloadCallback callback);

  using GetNextTrackCallback = base::OnceCallback<void(
      const std::optional<FocusModeSoundsDelegate::Track>&)>;
  void GetNextTrack(GetNextTrackCallback callback);

  const std::vector<std::unique_ptr<Playlist>>& soundscape_playlists() const {
    return soundscape_playlists_;
  }
  const std::vector<std::unique_ptr<Playlist>>& youtube_music_playlists()
      const {
    return youtube_music_playlists_;
  }

  const SelectedPlaylist& selected_playlist() const {
    return selected_playlist_;
  }

  focus_mode_util::SoundType sound_type() const { return sound_type_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Toggles a playlist with the same id as the `playlist_data` to select or
  // deselect based on its previous state.
  void TogglePlaylist(const SelectedPlaylist& playlist_data);

  // Download images by providing urls. `update_sounds_view_callback` will be
  // called only when finishing downloading all non-empty thumbnails for the
  // Soundscape type or the YouTube Music type of playlists; however, if
  // `ImageDownloader` doesn't exists or if there is an empty thumbnail
  // downloaded, `update_sounds_view_callback` will be not triggered.
  void DownloadPlaylistsForType(
      const bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback);

  void UpdateFromUserPrefs();

  // Sets the failure callback for all YouTube Music API requests. This callback
  // is used to update the specific UIs that are dependent on the account
  // premium status.
  void SetYouTubeMusicFailureCallback(base::RepeatingClosure callback);

 private:
  void SaveUserPref();
  void ResetSelectedPlaylist();
  void SelectPlaylist(const SelectedPlaylist& playlist_data);

  void OnAllThumbnailsDownloaded(
      bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback,
      std::vector<std::unique_ptr<Playlist>> sorted_playlists);

  std::unique_ptr<FocusModeSoundsDelegate> soundscape_delegate_;
  std::unique_ptr<FocusModeYouTubeMusicDelegate> youtube_music_delegate_;

  std::vector<std::unique_ptr<Playlist>> soundscape_playlists_;
  std::vector<std::unique_ptr<Playlist>> youtube_music_playlists_;

  SelectedPlaylist selected_playlist_;
  focus_mode_util::SoundType sound_type_ =
      focus_mode_util::SoundType::kSoundscape;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FocusModeSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
