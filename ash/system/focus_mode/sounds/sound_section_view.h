// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUND_SECTION_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUND_SECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace ash {

namespace {
class PlaylistView;
}  // namespace

class PillButton;

// These are views that represent a list of playlists and we toggle between
// `Focus Sounds` or `YouTube Music` sound sections using the slider button in
// the focus panel. When a non-premium user toggles to show the `YouTube Music`
// sound section, we will create and show the alternate view instead.
class ASH_EXPORT SoundSectionView : public views::FlexLayoutView {
  METADATA_HEADER(SoundSectionView, views::FlexLayoutView)

 public:
  SoundSectionView();
  SoundSectionView(const SoundSectionView&) = delete;
  SoundSectionView& operator=(const SoundSectionView&) = delete;
  ~SoundSectionView() override;

  // Update the contents in `playlist_view_list_`, e.g. the image view, title,
  // id of a playlist.
  void UpdateContents(
      const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
          data);

  // True to show `alternate_view_`; otherwise, show `playlist_views_container_`
  // instead.
  void ShowAlternateView(bool show_alternate_view);
  void SetAlternateView(std::unique_ptr<views::BoxLayoutView> alternate_view);

 protected:
  void CreatePlaylistViewsContainer();

  std::vector<PlaylistView*> playlist_view_list_;
  raw_ptr<views::BoxLayoutView> playlist_views_container_ = nullptr;

  // For a non-premium users, the "YouTube Music" `playlist_views_container_`
  // will not be populated. For this case, we will set an alternate view (e.g. a
  // non-premium view defined in `FocusModeSoundsView`) and show it instead.
  raw_ptr<views::BoxLayoutView> alternate_view_ = nullptr;

  raw_ptr<PillButton> learn_more_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUND_SECTION_VIEW_H_
