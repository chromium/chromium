// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include <string>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"

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

  const std::u16string& GetText() {
    return GetMediaStringViewTextLabel()->GetText();
  }
};

TEST_F(MediaStringViewTest, ShowMediaTitleAndArtist) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaMetadataChanged(metadata);

  const std::u16string expected_text = u"title \u2022 artist";
  EXPECT_EQ(GetMediaStringViewTextLabel()->GetText(), expected_text);
}

TEST_F(MediaStringViewTest, TextContainerFitsWidthOfShortText) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_EQ(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            GetMediaStringViewTextContainer()->GetPreferredSize().width());
}

TEST_F(MediaStringViewTest, TextContainerHasMaxWidthWithLongText) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_EQ(kMediaStringMaxWidthDip,
            GetMediaStringViewTextContainer()->GetPreferredSize().width());
}

TEST_F(MediaStringViewTest, HasNoAnimationWithShortText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, HasAnimationWithLongText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, ShouldStopAndStartAnimationWhenTextChanges) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to another long text.
  metadata.title = u"Another super duper long title";
  metadata.artist = u"Another super duper long artist name";
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, ShouldStartAndStopAnimationWhenTextChanges) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to long text.
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  // Change to short text.
  metadata.title = u"title";
  metadata.artist = u"name";
  SimulateMediaMetadataChanged(metadata);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, PauseMediaWillNotStopAnimationWithLongText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);

  EXPECT_FALSE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
  EXPECT_TRUE(
      GetMediaStringViewTextLabel()->layer()->GetAnimator()->is_animating());
}

TEST_F(MediaStringViewTest, HasNoMaskLayerWithShortText) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  for (auto* view : GetContainerViews())
    views::test::RunScheduledLayout(view);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_TRUE(
      GetMediaStringViewTextContainer()->layer()->gradient_mask().IsEmpty());
}

TEST_F(MediaStringViewTest, HasMaskLayerWithLongText) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  for (auto* view : GetContainerViews())
    views::test::RunScheduledLayout(view);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_FALSE(
      GetMediaStringViewTextContainer()->layer()->gradient_mask().IsEmpty());
}

TEST_F(MediaStringViewTest, MaskLayerShouldUpdate) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  for (auto* view : GetContainerViews())
    views::test::RunScheduledLayout(view);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_TRUE(
      GetMediaStringViewTextContainer()->layer()->gradient_mask().IsEmpty());

  // Change to long text.
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  for (auto* view : GetContainerViews())
    views::test::RunScheduledLayout(view);

  EXPECT_GT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_FALSE(
      GetMediaStringViewTextContainer()->layer()->gradient_mask().IsEmpty());

  // Change to short text.
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaMetadataChanged(metadata);
  // Force re-layout.
  for (auto* view : GetContainerViews())
    views::test::RunScheduledLayout(view);

  EXPECT_LT(GetMediaStringViewTextLabel()
                ->GetPreferredSize(views::SizeBounds(
                    GetMediaStringViewTextLabel()->width(), {}))
                .width(),
            kMediaStringMaxWidthDip);
  EXPECT_TRUE(
      GetMediaStringViewTextContainer()->layer()->gradient_mask().IsEmpty());
}

TEST_F(MediaStringViewTest, ShowWhenMediaIsPlaying) {
  SetAmbientShownAndWaitForWidgets();
  EXPECT_FALSE(GetMediaStringView()->GetVisible());

  // Sets media playstate for the current session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, DoNotShowWhenMediaIsPaused) {
  SetAmbientShownAndWaitForWidgets();
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
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  // Simulates active and playing media session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verifies media string is hidden.
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, ShouldHasDifferentTransform) {
  SetAmbientShownAndWaitForWidgets();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  SimulateMediaMetadataChanged(metadata);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());

  // It is theoretically that the transforms could be the same in two
  // consecutive updates, therefore we test with two updates.
  gfx::Transform transform1 =
      GetMediaStringView()->layer()->GetTargetTransform();
  FastForwardByPhotoRefreshInterval();
  FastForwardByPhotoRefreshInterval();
  gfx::Transform transform2 =
      GetMediaStringView()->layer()->GetTargetTransform();
  EXPECT_NE(transform1, transform2);
}

}  // namespace ash
