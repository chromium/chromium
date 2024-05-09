// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

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

  bool selected_playlist() const { return selected_playlist_; }

  // Download images by providing urls. `update_sounds_view_callback` will be
  // called only when finishing downloading all non-empty thumbnails for the
  // Soundscape type or the YouTube Music type of playlists; however, if
  // `ImageDownloader` doesn't exists or if there is an empty thumbnail
  // downloaded, `update_sounds_view_callback` will be not triggered.
  void DownloadPlaylistsForType(
      const bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback);

 private:
  // Invoked upon completion of the `thumbnail` download, `thumbnail` can be a
  // null image if the download attempt from the url failed.
  void OnOneThumbnailDownloaded(
      base::OnceCallback<void(std::unique_ptr<Playlist>)> barrier_callback,
      std::string id,
      std::string title,
      const gfx::ImageSkia& thumbnail);
  void OnAllThumbnailsDownloaded(
      bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback,
      std::vector<std::unique_ptr<Playlist>> playlists);

  std::vector<std::unique_ptr<Playlist>> soundscape_playlists_;
  std::vector<std::unique_ptr<Playlist>> youtube_music_playlists_;

  // TODO: Replace this with actual selected playlist information.
  bool selected_playlist_ = false;

  base::WeakPtrFactory<FocusModeSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
