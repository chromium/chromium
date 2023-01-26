// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

class MediaStringViewTest : public AmbientAshTestBase {
 public:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
    DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
    SetDecodedPhotoColor(SK_ColorBLACK);
  }

  void TearDown() override {
    CloseAmbientScreen();
    AmbientAshTestBase::TearDown();
  }
};

TEST_F(MediaStringViewTest, ShowMediaStringViewWithShortText) {
  ShowAmbientScreen();
  DisableJitter();
  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaMetadataChanged(metadata);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  // Force re-layout.
  for (auto* view : GetContainerViews()) {
    views::test::RunScheduledLayout(view);
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "media_string_view_with_short_text", /*revision_number=*/0,
      GetMediaStringView()));
}

TEST_F(MediaStringViewTest, ShowMediaStringViewWithShortTextDarkMode) {
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  ShowAmbientScreen();
  DisableJitter();
  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";

  SimulateMediaMetadataChanged(metadata);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  // Force re-layout.
  for (auto* view : GetContainerViews()) {
    views::test::RunScheduledLayout(view);
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "media_string_view_with_short_text_dark_mode",
      /*revision_number=*/0, GetMediaStringView()));
}

TEST_F(MediaStringViewTest, ShowMediaStringViewWithLongText) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ShowAmbientScreen();
  DisableJitter();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = u"A super duper long title";
  metadata.artist = u"A super duper long artist name";

  SimulateMediaMetadataChanged(metadata);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  // Force re-layout.
  for (auto* view : GetContainerViews()) {
    views::test::RunScheduledLayout(view);
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "media_string_view_with_long_text", /*revision_number=*/1,
      GetMediaStringView()));
}
}  // namespace ash
