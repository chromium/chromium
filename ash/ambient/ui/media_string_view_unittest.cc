// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"

namespace ash {

class MediaStringViewTest : public AmbientAshTestBase {
 public:
  MediaStringViewTest() : AmbientAshTestBase() {}
  ~MediaStringViewTest() override = default;

  // AmbientAshTestBase:
  void SetUp() override {
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }

  void TearDown() override {
    CloseAmbientScreen();
    AmbientAshTestBase::TearDown();
  }

  const base::string16& GetText() {
    return GetMediaStringViewTextLabel()->GetText();
  }
};

TEST_F(MediaStringViewTest, ShowMediaTitleAndArtist) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaMetadataChanged(metadata);

  const base::string16 expected_text = base::UTF8ToUTF16("title \u00B7 artist");
  EXPECT_EQ(GetMediaStringViewTextLabel()->GetText(), expected_text);
}

TEST_F(MediaStringViewTest, TextContainerFitsWidthOfShortText) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_EQ(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            GetMediaStringViewTextContainer()->GetPreferredSize().width());
}

TEST_F(MediaStringViewTest, TextContainerHasMaxWidthWithLongText) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_EQ(kMediaStringMaxWidthDip,
            GetMediaStringViewTextContainer()->GetPreferredSize().width());
}

TEST_F(MediaStringViewTest, HasNoAnimationWithShortText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, HasAnimationWithLongText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, ShouldStopAndStartAnimationWhenTextChanges) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to another long text.
  metadata.title = base::ASCIIToUTF16("Another super duper long title");
  metadata.artist = base::ASCIIToUTF16("Another super duper long artist name");
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, ShouldStartAndStopAnimationWhenTextChanges) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to long text.
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to short text.
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("name");
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, PauseMediaWillStopAnimationWithLongText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, HasNoMaskLayerWithShortText) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  container_view()->Layout();

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_FALSE(GetMediaStringViewTextContainer()->layer()->layer_mask_layer());
}

TEST_F(MediaStringViewTest, HasMaskLayerWithLongText) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  container_view()->Layout();

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_TRUE(GetMediaStringViewTextContainer()->layer()->layer_mask_layer());
}

TEST_F(MediaStringViewTest, MaskLayerShouldUpdate) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  container_view()->Layout();

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_FALSE(GetMediaStringViewTextContainer()->layer()->layer_mask_layer());

  // Change to long text.
  metadata.title = base::ASCIIToUTF16("A super duper long title");
  metadata.artist = base::ASCIIToUTF16("A super duper long artist name");

  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  container_view()->Layout();

  EXPECT_GT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_TRUE(GetMediaStringViewTextContainer()->layer()->layer_mask_layer());

  // Change to short text.
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  container_view()->Layout();

  EXPECT_LT(GetMediaStringViewTextLabel()->GetPreferredSize().width(),
            kMediaStringMaxWidthDip);
  EXPECT_FALSE(GetMediaStringViewTextContainer()->layer()->layer_mask_layer());
}

TEST_F(MediaStringViewTest, ShowWhenMediaIsPlaying) {
  ShowAmbientScreen();
  EXPECT_FALSE(GetMediaStringView()->GetVisible());

  // Sets media playstate for the current session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, DoNotShowWhenMediaIsPaused) {
  ShowAmbientScreen();
  EXPECT_FALSE(GetMediaStringView()->GetVisible());

  // Sets media playstate for the current session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());

  // Simulates the ongoing media paused.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, DoNotShowOnLockScreenIfPrefIsDisabled) {
  // Disables user preference for media controls.
  PrefService* pref =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  pref->SetBoolean(prefs::kLockScreenMediaControlsEnabled, false);
  // Simulates Ambient Mode shown on lock-screen.
  LockScreen();
  FastForwardToInactivity();
  FastForwardTiny();

  // Simulates active and playing media session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verifies media string is hidden.
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, ShouldHasDifferentTransform) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());

  // It is theoretically that the transforms could be the same in two
  // consecutive updates, therefore we test with two updates.
  gfx::Transform transform1 =
      GetMediaStringView()->layer()->GetTargetTransform();
  FastForwardToNextImage();
  FastForwardToNextImage();
  gfx::Transform transform2 =
      GetMediaStringView()->layer()->GetTargetTransform();
  EXPECT_NE(transform1, transform2);
}

}  // namespace ash
