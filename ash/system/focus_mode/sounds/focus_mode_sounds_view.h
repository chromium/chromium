// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/rounded_container.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace ash {

class TabSliderButton;

namespace {
// A view contains an image of a playlist and a title.
class PlaylistView;
}  // namespace

// This view will be added on `FocusModeDetailedView` below the task container
// row to show playlists of YouTube music. Clicking two tab slider buttons will
// display two different types of music. Each playlist view will show a
// thumbnail of the playlist cover, a title of the playlist and some media
// control icons.
class ASH_EXPORT FocusModeSoundsView : public RoundedContainer {
  METADATA_HEADER(FocusModeSoundsView, RoundedContainer)

 public:
  FocusModeSoundsView();
  FocusModeSoundsView(const FocusModeSoundsView&) = delete;
  FocusModeSoundsView& operator=(const FocusModeSoundsView&) = delete;
  ~FocusModeSoundsView() override;

 private:
  // Update this view based on `is_soundscape_type`.
  void UpdateSoundsView(bool is_soundscape_type);

  // Creates `soundscape_button_` and `youtube_music_button_`.
  void CreateTabSliderButtons();

  // Called to show YouTube Music soundscape playlists.
  void OnSoundscapeButtonToggled();

  // Called to show personalized YouTube Music playlists.
  void OnYoutubeMusicButtonToggled();

  // The slider buttons on the sound view.
  raw_ptr<TabSliderButton> soundscape_button_ = nullptr;
  raw_ptr<TabSliderButton> youtube_music_button_ = nullptr;

  // Container views contains a list of `PlaylistView`.
  raw_ptr<views::BoxLayoutView> soundscape_container_;
  raw_ptr<views::BoxLayoutView> youtube_music_container_;

  // A list ptrs of `PlaylistView` which have been added into
  // `soundscape_container_` or `youtube_music_container_`.
  std::vector<PlaylistView*> soundscape_playlist_view_list_;
  std::vector<PlaylistView*> youtube_music_playlist_view_list_;

  base::WeakPtrFactory<FocusModeSoundsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_
