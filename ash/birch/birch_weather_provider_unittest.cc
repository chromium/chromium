// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"

namespace ash {

// A data provider that does nothing.
class StubBirchDataProvider : public BirchDataProvider {
 public:
  StubBirchDataProvider() = default;
  ~StubBirchDataProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {}
};

class StubBirchClient : public BirchClient {
 public:
  StubBirchClient() = default;
  ~StubBirchClient() override = default;

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override { return &provider_; }
  BirchDataProvider* GetFileSuggestProvider() override { return &provider_; }
  BirchDataProvider* GetRecentTabsProvider() override { return &provider_; }
  BirchDataProvider* GetReleaseNotesProvider() override { return &provider_; }

  void WaitForRefreshTokens(base::OnceClosure callback) override {
    did_wait_for_refresh_tokens_ = true;
    std::move(callback).Run();
  }
  base::FilePath GetRemovedItemsFilePath() override { return base::FilePath(); }

  StubBirchDataProvider provider_;
  bool did_wait_for_refresh_tokens_ = false;
};

class BirchWeatherProviderTest : public AshTestBase {
 public:
  BirchWeatherProviderTest() {
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
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

  raw_ptr<FakeAmbientBackendControllerImpl> ambient_backend_controller_;
  std::unique_ptr<TestImageDownloader> image_downloader_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

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
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
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
  EXPECT_TRUE(birch_client.did_wait_for_refresh_tokens_);

  // Weather data was fetched.
  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));

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
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
  run_loop.Run();

  // Weather was not fetched.
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

TEST_F(BirchWeatherProviderTest, GetWeatherInCelsius) {
  auto* birch_model = Shell::Get()->birch_model();

  WeatherInfo info;
  info.condition_description = "Cloudy";
  info.condition_icon_url = "https://fake-icon-url";
  info.temp_f = 70.0f;
  info.show_celsius = true;
  ambient_backend_controller_->SetWeatherInfo(std::move(info));

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(/*is_post_login=*/false,
                                     run_loop.QuitClosure());
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title());
  EXPECT_EQ(u"21\xB0 C", weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
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

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
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

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
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
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));

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
  EXPECT_EQ(u"73\xB0 F", updated_weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
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
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature());
  weather_items[0].LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));

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

}  // namespace ash
