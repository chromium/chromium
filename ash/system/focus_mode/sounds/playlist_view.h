// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
}

namespace ash {

class PlaylistImageButton;

class ASH_EXPORT PlaylistView : public views::BoxLayoutView {
  METADATA_HEADER(PlaylistView, views::BoxLayoutView)

 public:
  using TogglePlaylistCallback = base::RepeatingCallback<void(
      const focus_mode_util::SelectedPlaylist& playlist_data)>;

  PlaylistView(focus_mode_util::SoundType type,
               TogglePlaylistCallback toggle_playlist_callback);
  PlaylistView(const PlaylistView&) = delete;
  PlaylistView& operator=(const PlaylistView&) = delete;
  ~PlaylistView() override;

  const focus_mode_util::SelectedPlaylist& playlist_data() {
    return playlist_data_;
  }

  void UpdateContents(uint8_t position,
                      const FocusModeSoundsController::Playlist& playlist);
  void SetState(focus_mode_util::SoundState state);

 private:
  // Called when the `playlist_image_button_` is toggled by the user.
  void OnPlaylistViewToggled();

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<PlaylistImageButton> playlist_image_button_ = nullptr;
  focus_mode_util::SelectedPlaylist playlist_data_;

  TogglePlaylistCallback toggle_playlist_callback_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_VIEW_H_
