// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

class AmbientInfoViewTest : public AmbientAshTestBase {
 public:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
    SetDecodedPhotoColor(SK_ColorYELLOW);
    // Set the photo size to match the display size.
    SetDecodedPhotoSize(800, 600);
  }

  void TearDown() override {
    CloseAmbientScreen();
    AmbientAshTestBase::TearDown();
  }
};

TEST_F(AmbientInfoViewTest, ShowAmbientInfoView) {
  ShowAmbientScreen();
  DisableJitter();

  WeatherInfo info;
  info.show_celsius = true;
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  backend_controller()->SetWeatherInfo(info);
  FastForwardToRefreshWeather();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "ambient_info_view", /*revision_number=*/0, GetAmbientInfoView()));
}
}  // namespace ash
