// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"

#include <ostream>
#include <utility>

#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "ash/system/focus_mode/sounds/soundscape/test/fake_soundscapes_downloader.h"
#include "ash/system/focus_mode/sounds/soundscape/test/test_data.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestUuid[] = "e3db3b31-45d5-4fe0-a8f0-6ec9eeb95678";

void TrackCallback(base::RunLoop* run_loop,
                   std::optional<FocusModeSoundsDelegate::Track>* result,
                   const std::optional<FocusModeSoundsDelegate::Track>& track) {
  *result = track;
  run_loop->Quit();
}

SoundscapeTrack TestSong(int i) {
  return SoundscapeTrack(
      /*name=*/base::StringPrintf("%d", i),
      /*path=*/base::StringPrintf("tracks/blah/foo%d.bar", i));
}

class FocusModeSoundscapeDelegateTest : public testing::Test {
 public:
  FocusModeSoundscapeDelegateTest() = default;

  void SetUp() override {
    auto downloader = std::make_unique<FakeSoundscapesDownloader>();
    fake_downloader_ = downloader.get();
    delegate_ =
        std::make_unique<FocusModeSoundscapeDelegate>(std::move(downloader));
  }

  void TearDown() override {
    fake_downloader_ = nullptr;
    delegate_.reset();
  }

  FocusModeSoundscapeDelegate* delegate() { return delegate_.get(); }

  FakeSoundscapesDownloader* fake_downloader() { return fake_downloader_; }

  std::optional<FocusModeSoundsDelegate::Track> GetOneTrack() {
    base::RunLoop run_loop;
    std::optional<FocusModeSoundsDelegate::Track> result;
    delegate()->GetNextTrack(
        kTestUuid, base::BindOnce(&TrackCallback, base::Unretained(&run_loop),
                                  base::Unretained(&result)));
    run_loop.Run();
    return result;
  }

 private:
  raw_ptr<FakeSoundscapesDownloader> fake_downloader_ = nullptr;
  std::unique_ptr<FocusModeSoundscapeDelegate> delegate_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(FocusModeSoundscapeDelegateTest, Construct) {
  EXPECT_TRUE(delegate());
}

TEST_F(FocusModeSoundscapeDelegateTest, GetNextTrack) {
  SoundscapePlaylist test_playlist;
  test_playlist.uuid = base::Uuid::ParseLowercase(kTestUuid);
  test_playlist.tracks.push_back(TestSong(0));
  test_playlist.tracks.push_back(TestSong(1));
  test_playlist.tracks.push_back(TestSong(2));
  test_playlist.tracks.push_back(TestSong(3));
  fake_downloader()->SetPlaylistResponse(test_playlist);

  {
    // TODO(b/342467806): Delegate expects to fetch playlists before tracks.
    base::RunLoop config_loop;
    delegate()->GetPlaylists(
        base::IgnoreArgs<const std::vector<FocusModeSoundsDelegate::Playlist>&>(
            config_loop.QuitClosure()));
    config_loop.Run();
  }

  // Verify we see each track once if we play the playlist once.
  std::vector<FocusModeSoundsDelegate::Track> tracks;
  for (int i = 0; i < 4; i++) {
    std::optional<FocusModeSoundsDelegate::Track> result = GetOneTrack();
    ASSERT_TRUE(result) << i;
    tracks.push_back(*result);
  }

  EXPECT_THAT(tracks, testing::SizeIs(4));
  EXPECT_THAT(tracks,
              testing::UnorderedElementsAre(
                  testing::Field(&FocusModeSoundsDelegate::Track::title, "0"),
                  testing::Field(&FocusModeSoundsDelegate::Track::title, "1"),
                  testing::Field(&FocusModeSoundsDelegate::Track::title, "2"),
                  testing::Field(&FocusModeSoundsDelegate::Track::title, "3")));

  // Verify that the last track does not appear in the next n-1 plays.
  FocusModeSoundsDelegate::Track last_track = tracks.back();

  tracks.clear();
  for (int i = 0; i < 3; i++) {
    std::optional<FocusModeSoundsDelegate::Track> result = GetOneTrack();
    ASSERT_TRUE(result) << i;
    tracks.push_back(*result);
  }

  EXPECT_THAT(tracks, testing::SizeIs(3));
  EXPECT_THAT(tracks, testing::Each(testing::Not(last_track)));

  // The 8th track played is the same as the 4th.
  EXPECT_THAT(GetOneTrack(), testing::Optional(testing::Eq(last_track)));
}

}  // namespace

void PrintTo(const FocusModeSoundsDelegate::Track& track, std::ostream* os) {
  *os << track.title << " " << track.artist << " " << track.source << " "
      << track.thumbnail_url << " " << track.source_url;
}

}  // namespace ash
