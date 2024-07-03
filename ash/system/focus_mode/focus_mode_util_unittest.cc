// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::focus_mode_util {

// Verify that missing `id` or invalid playlist type results in an empty
// string.
TEST(FocusModeUtilTests, VerifyInvalidSourceTitle) {
  SelectedPlaylist selected_playlist;
  EXPECT_TRUE(GetSourceTitleForMediaControls(selected_playlist).empty());
  selected_playlist.id = "id0";
  EXPECT_TRUE(GetSourceTitleForMediaControls(selected_playlist).empty());
}

// Verify that having a missing playlist title will still return the playlist
// type as a string.
TEST(FocusModeUtilTests, VerifySourceTitleWithMissingPlaylistTitle) {
  SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = SoundType::kYouTubeMusic;
  EXPECT_EQ(GetSourceTitleForMediaControls(selected_playlist), "YouTube Music");
}

// Verify a fully formed YTM string.
TEST(FocusModeUtilTests, VerifyYTMSourceTitle) {
  SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = SoundType::kYouTubeMusic;
  selected_playlist.title = "Playlist Title";
  EXPECT_EQ(GetSourceTitleForMediaControls(selected_playlist),
            "YouTube Music ᐧ Playlist Title");
}

// Verify a fully formed Soundscape string.
TEST(FocusModeUtilTests, VerifySoundscapeSourceTitle) {
  SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = SoundType::kSoundscape;
  selected_playlist.title = "Playlist Title";
  EXPECT_EQ(GetSourceTitleForMediaControls(selected_playlist),
            "Focus sounds ᐧ Playlist Title");
}

}  // namespace ash::focus_mode_util
