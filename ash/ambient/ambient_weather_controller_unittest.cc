// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_weather_controller.h"

#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using AmbientWeatherControllerTest = AmbientAshTestBase;

TEST_F(AmbientWeatherControllerTest, RefreshesWeather) {
  auto* model = weather_controller()->weather_model();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_TRUE(model->weather_condition_icon().isNull());

  WeatherInfo info;
  info.show_celsius = true;
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  backend_controller()->SetWeatherInfo(info);

  // Check location permission is granted.
  EXPECT_TRUE(SimpleGeolocationProvider::GetInstance()
                  ->IsGeolocationUsageAllowedForSystem());

  auto weather_refresher = weather_controller()->CreateScopedRefresher();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(model->show_celsius());
  EXPECT_FALSE(model->weather_condition_icon().isNull());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), 70.0f);

  // Refresh weather again after time passes.
  info.show_celsius = false;
  info.temp_f = -70.0f;
  backend_controller()->SetWeatherInfo(info);

  FastForwardByWeatherRefreshInterval();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), -70.0f);

  info.show_celsius = true;
  info.temp_f = 70.0f;
  backend_controller()->SetWeatherInfo(info);
  // Should stop refreshing after the `weather_refresher` is destroyed.
  weather_refresher.reset();
  FastForwardByWeatherRefreshInterval();
  // The old info should hold in the model since we're not refreshing.
  EXPECT_FALSE(model->show_celsius());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), -70.0f);
}

TEST_F(AmbientWeatherControllerTest, RespectsSystemLocationPermission) {
  auto* model = weather_controller()->weather_model();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_TRUE(model->weather_condition_icon().isNull());

  // Check location permission is enabled by default.
  EXPECT_TRUE(SimpleGeolocationProvider::GetInstance()
                  ->IsGeolocationUsageAllowedForSystem());

  WeatherInfo info;
  info.show_celsius = true;
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  backend_controller()->SetWeatherInfo(info);

  // Disable location permission and check the weather model will not get
  // updated.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);

  auto weather_refresher = weather_controller()->CreateScopedRefresher();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_TRUE(model->weather_condition_icon().isNull());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), 0.0f);

  // Check again on next interval timelapse.
  FastForwardByWeatherRefreshInterval();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_TRUE(model->weather_condition_icon().isNull());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), 0.0f);

  // Enable location permission for system services and check the weather model
  // will get updated.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kOnlyAllowedForSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model->show_celsius());
  EXPECT_FALSE(model->weather_condition_icon().isNull());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), 70.0f);

  // Enable location for all clients and check it's continued fetching new
  // weather models.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kAllowed);

  info.show_celsius = false;
  info.temp_f = -70.0f;
  backend_controller()->SetWeatherInfo(info);

  FastForwardByWeatherRefreshInterval();
  EXPECT_FALSE(model->show_celsius());
  EXPECT_FALSE(model->weather_condition_icon().isNull());
  EXPECT_FLOAT_EQ(model->temperature_fahrenheit(), -70.0f);
}

}  // namespace
}  // namespace ash
