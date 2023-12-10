// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <optional>
#include <tuple>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"

namespace ash {

namespace {

using TestParams = std::tuple<std::optional<media_session::MediaMetadata>,
                              /*dark_mode=*/bool,
                              /*rtl=*/bool,
                              /*jelly=*/bool>;

std::array<std::optional<media_session::MediaMetadata>, 3>
GetMediaMedataVariations() {
  media_session::MediaMetadata short_metadata;
  short_metadata.artist = u"artist";
  short_metadata.title = u"title";

  media_session::MediaMetadata long_metadata;
  long_metadata.artist = u"A super duper long artist name";
  long_metadata.title = u"A super duper long title";

  return {std::move(short_metadata), std::move(long_metadata), std::nullopt};
}

bool IsDarkMode(const TestParams& param) {
  return std::get<1>(param);
}

bool IsRtl(const TestParams& param) {
  return std::get<2>(param);
}

bool IsJellyEnabled(const TestParams& param) {
  return std::get<3>(param);
}

const std::optional<media_session::MediaMetadata>& GetMediaMetadata(
    const TestParams& param) {
  return std::get<std::optional<media_session::MediaMetadata>>(param);
}

std::string GetName(const testing::TestParamInfo<TestParams>& param_info) {
  const std::optional<media_session::MediaMetadata>& metadata =
      GetMediaMetadata(param_info.param);
  std::string metadata_description_text;
  if (!metadata.has_value()) {
    metadata_description_text = "Nomedia";
  } else if (metadata->artist == u"artist") {
    metadata_description_text = "Shortmedia";
  } else {
    metadata_description_text = "Longmedia";
  }
  std::vector<std::string> attributes = {
      metadata_description_text,
      IsDarkMode(param_info.param) ? "Dark" : "Light",
      IsRtl(param_info.param) ? "Rtl" : "Ltr"};
  if (!IsJellyEnabled(param_info.param)) {
    attributes.push_back("PreJelly");
  }
  return base::StrCat(attributes);
}

}  // namespace

class AmbientSlideshowPixelTest
    : public AmbientAshTestBase,
      public testing::WithParamInterface<TestParams> {
 public:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams pixel_test_init_params;
    pixel_test_init_params.under_rtl = IsRtl(GetParam());
    return pixel_test_init_params;
  }

  void SetUp() override {
    scoped_features_.InitWithFeatureState(chromeos::features::kJelly,
                                          IsJellyEnabled(GetParam()));
    AmbientAshTestBase::SetUp();
    SetAmbientTheme(personalization_app::mojom::AmbientTheme::kSlideshow);
    GetSessionControllerClient()->set_show_lock_screen_views(true);
    DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));
    // Required for decoded image parameters below to exactly reflect what the
    // ambient photo pipeline produces.
    UseLosslessPhotoCompression(true);
    SetNextDecodedPhotoColor(SK_ColorGREEN);
    gfx::Size display_size = GetPrimaryDisplay().size();
    // Simplifies rendering to be consistent for pixel testing.
    SetDecodedPhotoSize(display_size.width(), display_size.height());
    // Set a very long photo refresh interval so the view we are looking
    // at does not animate out while screen capturing. By default two photo
    // views animate in/out to transition between photos every 60 seconds.
    AmbientUiModel::Get()->SetPhotoRefreshInterval(base::Hours(1));
    // Stop the clock and media info from moving around.
    DisableJitter();
  }

  void TearDown() override {
    CloseAmbientScreen();
    AmbientAshTestBase::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AmbientSlideshowPixelTest,
    testing::Combine(::testing::ValuesIn(GetMediaMedataVariations()),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Bool()),
    &GetName);

TEST_P(AmbientSlideshowPixelTest, ShowMediaStringView) {
  SetAmbientShownAndWaitForWidgets();

  const std::optional<media_session::MediaMetadata>& media_metadata =
      GetMediaMetadata(GetParam());

  if (media_metadata.has_value()) {
    SimulateMediaMetadataChanged(media_metadata.value());
    SimulateMediaPlaybackStateChanged(
        media_session::mojom::MediaPlaybackState::kPlaying);
  }

  // Revision 1 is with Jelly enabled. Revision 0 is with Jelly disabled to
  // guard against regressions until the flag is fully launched.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AmbientSlideshow",
      /*revision_number=*/IsJellyEnabled(GetParam()) ? 1 : 0,
      ash::Shell::GetPrimaryRootWindow()));
}

}  // namespace ash
