// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class FocusModeSoundsDelegate;

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

 private:
  void ResetSelectedPlaylist();
  void SelectPlaylist(const SelectedPlaylist& playlist_data);

  void OnAllThumbnailsDownloaded(
      bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback,
      std::vector<std::unique_ptr<Playlist>> playlists);

  std::unique_ptr<FocusModeSoundsDelegate> soundscape_delegate_;
  std::unique_ptr<FocusModeSoundsDelegate> youtube_music_delegate_;

  std::vector<std::unique_ptr<Playlist>> soundscape_playlists_;
  std::vector<std::unique_ptr<Playlist>> youtube_music_playlists_;

  SelectedPlaylist selected_playlist_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FocusModeSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
