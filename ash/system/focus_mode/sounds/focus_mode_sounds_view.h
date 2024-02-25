// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/rounded_container.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class TabSliderButton;

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
  // Creates `soundscape_button_` and `youtube_music_button_`.
  void CreateTabSliderButtons();

  // Called to show YouTube Music soundscape playlists.
  void OnSoundscapeButtonToggled();

  // Called to show personalized YouTube Music playlists.
  void OnYoutubeMusicButtonToggled();

  // The slider buttons on the sound view.
  raw_ptr<TabSliderButton> soundscape_button_ = nullptr;
  raw_ptr<TabSliderButton> youtube_music_button_ = nullptr;

  base::WeakPtrFactory<FocusModeSoundsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_
