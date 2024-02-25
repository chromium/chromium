// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/glanceable_info_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

// using GlanceableInfoViewTest = AmbientAshTestBase;

class GlanceableInfoViewTest : public AmbientAshTestBase {
 public:
  GlanceableInfoViewTest() : AmbientAshTestBase() {}
  ~GlanceableInfoViewTest() override = default;

  void SetUp() override {
    AmbientAshTestBase::SetUp();

    // Set test weather info.
    WeatherInfo info;
    info.show_celsius = true;
    info.condition_icon_url = "https://fake-icon-url";
    info.temp_f = 70.0f;
    backend_controller()->SetWeatherInfo(info);

    weather_model_ = weather_controller()->weather_model();
    weather_refresher_ = weather_controller()->CreateScopedRefresher();
  }

  void TearDown() override {
    weather_refresher_.reset();
    weather_model_ = nullptr;
    AmbientAshTestBase::TearDown();
  }

 protected:
  raw_ptr<AmbientWeatherModel> weather_model_;
  std::unique_ptr<AmbientWeatherController::ScopedRefresher>
      weather_refresher_ = nullptr;
};

TEST_F(GlanceableInfoViewTest, WeatherInfoIsShown) {
  // Geolocation should be allowed by default.
  ASSERT_EQ(
      GeolocationAccessLevel::kAllowed,
      SimpleGeolocationProvider::GetInstance()->GetGeolocationAccessLevel());

  // Wait for the initial weather fetch and check the weather model is updated.
  FastForwardByWeatherRefreshInterval();
  ASSERT_FALSE(weather_model_->weather_condition_icon().isNull());
  ASSERT_FLOAT_EQ(70.0f, weather_model_->temperature_fahrenheit());

  // Launch screensaver.
  SetAmbientShownAndWaitForWidgets();

  // Check that the weather is shown.
  AmbientInfoView* ambient_info = GetAmbientInfoView();
  CHECK_NE(nullptr, ambient_info);
  GlanceableInfoView* weather_info_view =
      ambient_info->GetGlanceableInfoViewForTesting();
  EXPECT_TRUE(weather_info_view->IsWeatherConditionIconSetForTesting());
  EXPECT_TRUE(weather_info_view->IsTemperatureSetForTesting());
}

TEST_F(GlanceableInfoViewTest, WeatherInfoIsHiddenWhenGeolocationIsOff) {
  // Geolocation should be allowed by default.
  ASSERT_EQ(
      GeolocationAccessLevel::kAllowed,
      SimpleGeolocationProvider::GetInstance()->GetGeolocationAccessLevel());

  // Wait for the initial weather fetch and check the weather model is updated.
  FastForwardByWeatherRefreshInterval();
  ASSERT_FALSE(weather_model_->weather_condition_icon().isNull());
  ASSERT_FLOAT_EQ(70.0f, weather_model_->temperature_fahrenheit());

  // Launch screensaver.
  SetAmbientShownAndWaitForWidgets();

  // Check that weather info is shown.
  AmbientInfoView* ambient_info = GetAmbientInfoView();
  CHECK_NE(nullptr, ambient_info);
  GlanceableInfoView* weather_info_view =
      ambient_info->GetGlanceableInfoViewForTesting();
  EXPECT_TRUE(weather_info_view->IsWeatherConditionIconSetForTesting());
  EXPECT_TRUE(weather_info_view->IsTemperatureSetForTesting());

  // Disable geolocation and check the weather info has disappeared.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(weather_info_view->IsWeatherConditionIconSetForTesting());
  EXPECT_FALSE(weather_info_view->IsTemperatureSetForTesting());

  // Re-enable geolocation permission and check that weather is shown again.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kAllowed);
  base::RunLoop().RunUntilIdle();
  FastForwardByWeatherRefreshInterval();

  ASSERT_FALSE(weather_model_->weather_condition_icon().isNull());
  ASSERT_FLOAT_EQ(70.0f, weather_model_->temperature_fahrenheit());
  EXPECT_TRUE(weather_info_view->IsWeatherConditionIconSetForTesting());
  EXPECT_TRUE(weather_info_view->IsTemperatureSetForTesting());
}

}  // namespace ash
