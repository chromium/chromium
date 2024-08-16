// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/birch/birch_icon_cache.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/stub_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"

namespace ash {
namespace {

BirchWeatherProvider* GetWeatherProvider() {
  return static_cast<BirchWeatherProvider*>(
      Shell::Get()->birch_model()->GetWeatherProviderForTest());
}

class BirchWeatherProviderTest : public AshTestBase {
 public:
  BirchWeatherProviderTest() : clock_override_(&GetTestTime, nullptr, nullptr) {
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
    // Ensure the time is morning (7 AM) so weather will be fetched.
    SetTestTime(base::Time::Now().LocalMidnight() + base::Hours(7));
  }
  ~BirchWeatherProviderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    image_downloader_ = std::make_unique<ash::TestImageDownloader>();

    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        nullptr);
    auto ambient_backend_controller =
        std::make_unique<FakeAmbientBackendControllerImpl>();
    ambient_backend_controller_ = ambient_backend_controller.get();
    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        std::move(ambient_backend_controller));
  }
  void TearDown() override {
    ambient_backend_controller_ = nullptr;
    image_downloader_.reset();
    AshTestBase::TearDown();
  }

  static base::Time GetTestTime() { return test_time_; }

  static void SetTestTime(base::Time test_time) { test_time_ = test_time; }

  raw_ptr<FakeAmbientBackendControllerImpl> ambient_backend_controller_;
  std::unique_ptr<TestImageDownloader> image_downloader_;

 private:
  base::subtle::ScopedTimeClockOverrides clock_override_;
  static base::Time test_time_;
  base::test::ScopedFeatureList feature_list_;
};

// static
base::Time BirchWeatherProviderTest::test_time_;

TEST_F(BirchWeatherProviderTest, GetWeather) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));
}

TEST_F(BirchWeatherProviderTest, GetWeatherUsesChromeOSWeatherClientId) {
  auto* birch_model = Shell::Get()->birch_model();

  // Set up fake weather.
  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  // Fetch birch data, which includes weather.
  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // Verify the controller was called with the correct weather client ID.
  std::optional<std::string> weather_client_id =
      ambient_backend_controller_->weather_client_id();
  ASSERT_TRUE(weather_client_id.has_value());
  EXPECT_EQ(*weather_client_id, "chromeos-system-ui");
}

TEST_F(BirchWeatherProviderTest, GetWeatherWaitsForRefreshTokens) {
  auto* birch_model = Shell::Get()->birch_model();
  StubBirchClient birch_client;
  birch_model->SetClientAndInit(&birch_client);

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The provider used the client to wait for refresh tokens.
  EXPECT_TRUE(birch_client.did_wait_for_refresh_tokens());

  // Weather data was fetched.
  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));

  birch_model->SetClientAndInit(nullptr);
}

TEST_F(BirchWeatherProviderTest, WeatherNotFetchedWhenGeolocationDisabled) {
  auto* birch_model = Shell::Get()->birch_model();

  // Set up fake backend weather.
  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  // Disable geolocation.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);

  // Fetch birch data.
  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // Weather was not fetched.
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherNotFetchedForStubUser) {
  auto* birch_model = Shell::Get()->birch_model();

  // Set up fake backend weather.
  WeatherInfo info;
  info.condition_description = "Sunny";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 72.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  // Simulate a stub user login.
  ClearLogin();
  SimulateUserLogin(user_manager::StubAccountId());

  // Fetch birch data.
  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The weather was not fetched.
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherNotFetchedInAfternoon) {
  auto* birch_model = Shell::Get()->birch_model();

  // Set up fake backend weather.
  WeatherInfo info;
  info.condition_description = "Sunny";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 72.0f;
  ambient_backend_controller_->SetWeatherInfo(info);

  // Simulate afternoon (2 PM).
  SetTestTime(base::Time::Now().LocalMidnight() + base::Hours(14));

  // Fetch birch data.
  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The weather was not fetched.
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, NoWeatherInfo) {
  auto* birch_model = Shell::Get()->birch_model();

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherWithNoIcon) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.show_celsius = false;
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherWithInvalidIcon) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "<invalid url>";
  info.show_celsius = false;
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The item exists and will use the backup icon.
  EXPECT_FALSE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherIconDownloadFailure) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake_icon_url";
  info.show_celsius = false;
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  image_downloader_->set_should_fail(true);

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The item exists and will use the backup icon.
  EXPECT_FALSE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherWithNoTemperature) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake_icon_url";
  info.show_celsius = false;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, WeatherWithNoDecription) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_icon_url = "https://fake_icon_url";
  info.show_celsius = false;
  info.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, RefetchWeather) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info1;
  info1.condition_description = "Cloudy";
  info1.condition_icon_url = "https://fake-icon-url";
  info1.show_celsius = false;
  info1.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info1);

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));

  // Ensure the cache isn't used.
  GetWeatherProvider()->ResetCacheForTest();

  WeatherInfo info2;
  info2.condition_description = "Sunny";
  info2.condition_icon_url = "https://fake-icon-url";
  info2.show_celsius = false;
  info2.temp_f = 73.0f;
  ambient_backend_controller_->SetWeatherInfo(info2);

  base::RunLoop run_loop2;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop2.QuitClosure());
  run_loop2.Run();

  auto& updated_weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, updated_weather_items.size());
  EXPECT_EQ(u"Sunny", updated_weather_items[0].title());
  EXPECT_FLOAT_EQ(73.f, updated_weather_items[0].temp_f());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));
}

TEST_F(BirchWeatherProviderTest, RefetchUsesCache) {
  auto* birch_model = Shell::Get()->birch_model();

  // Set up weather.
  WeatherInfo info1;
  info1.condition_description = "Cloudy";
  info1.condition_icon_url = "https://fake-icon-url";
  info1.show_celsius = false;
  info1.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info1);

  // Make an initial fetch.
  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  // The weather from `info1` is fetched.
  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());

  // Set up different weather.
  WeatherInfo info2;
  info2.condition_description = "Sunny";
  info2.condition_icon_url = "https://fake-icon-url";
  info2.show_celsius = false;
  info2.temp_f = 73.0f;
  ambient_backend_controller_->SetWeatherInfo(info2);

  // Make another request. This will hit the cache and not fetch `info2`.
  base::RunLoop run_loop2;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop2.QuitClosure());
  run_loop2.Run();

  // The data is from `info1` because it came from the cache.
  auto& updated_weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, updated_weather_items.size());
  EXPECT_EQ(u"Cloudy", updated_weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, updated_weather_items[0].temp_f());
}

TEST_F(BirchWeatherProviderTest, RefetchInvalidWeather) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info1;
  info1.condition_description = "Cloudy";
  info1.condition_icon_url = "https://fake-icon-url";
  info1.show_celsius = false;
  info1.temp_f = 70.0f;
  ambient_backend_controller_->SetWeatherInfo(info1);

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));

  // Ensure the cache isn't used.
  GetWeatherProvider()->ResetCacheForTest();

  WeatherInfo info2;
  info2.show_celsius = false;
  ambient_backend_controller_->SetWeatherInfo(info2);

  base::RunLoop run_loop2;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, AllowOneFetchAtATime) {
  auto* birch_model = Shell::Get()->birch_model();
  BirchWeatherProvider provider(birch_model);

  // Set up the ambient controller so it pauses on FetchWeather().
  ambient_backend_controller_->set_run_fetch_weather_callback(false);
  ASSERT_EQ(ambient_backend_controller_->fetch_weather_count(), 0);

  // Make two concurrent weather requests.
  provider.RequestBirchDataFetch();
  provider.RequestBirchDataFetch();

  // The backend only received one request to fetch weather.
  EXPECT_EQ(ambient_backend_controller_->fetch_weather_count(), 1);
}

TEST_F(BirchWeatherProviderTest, DisabledByPolicy) {
  auto* birch_model = Shell::Get()->birch_model();
  BirchWeatherProvider provider(birch_model);

  // Disable weather integration by policy, no weather should be fetched.
  auto* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  pref_service->SetList(prefs::kContextualGoogleIntegrationsConfiguration, {});

  provider.RequestBirchDataFetch();
  EXPECT_EQ(ambient_backend_controller_->fetch_weather_count(), 0);

  // Enable weather integration by policy, weather should be fetched.
  base::Value::List enabled_integrations;
  enabled_integrations.Append(prefs::kWeatherIntegrationName);
  pref_service->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                        std::move(enabled_integrations));

  provider.RequestBirchDataFetch();
  EXPECT_EQ(ambient_backend_controller_->fetch_weather_count(), 1);
}

}  // namespace
}  // namespace ash
