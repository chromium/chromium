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

// Verify the YTM source string.
TEST(FocusModeUtilTests, VerifyYTMSourceTitle) {
  SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = SoundType::kYouTubeMusic;
  EXPECT_EQ(GetSourceTitleForMediaControls(selected_playlist), "YouTube Music");
}

// Verify the Soundscape source string.
TEST(FocusModeUtilTests, VerifySoundscapeSourceTitle) {
  SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = SoundType::kSoundscape;
  EXPECT_EQ(GetSourceTitleForMediaControls(selected_playlist), "Focus sounds");
}

// Verify we get the correct `GetNextProgressStep` based on the current
// progress.
TEST(FocusModeUtilTests, VerifyGetNextProgressStep) {
  // Verify that when it starts, it just expecting the first step.
  EXPECT_EQ(GetNextProgressStep(0.0), 1);

  // Verify values slightly below, exactly at, and slightly above a threshold.
  EXPECT_EQ(GetNextProgressStep(0.499999), 60);
  EXPECT_EQ(GetNextProgressStep(0.5), 61);
  EXPECT_EQ(GetNextProgressStep(0.5000001), 61);

  // Test a progress value very close to max, which would expect the last step.
  EXPECT_EQ(GetNextProgressStep(0.999), 120);

  // Test that we clamp the step value to the last step.
  EXPECT_EQ(GetNextProgressStep(1.0), 120);
}

}  // namespace ash::focus_mode_util
