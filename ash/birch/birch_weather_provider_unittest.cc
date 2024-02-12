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
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class BirchWeatherProviderTest : public AshTestBase {
 public:
  BirchWeatherProviderTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
  }
  ~BirchWeatherProviderTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title);
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature);
  EXPECT_FALSE(weather_items[0].icon.IsEmpty());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title);
  EXPECT_EQ(u"21\xB0 C", weather_items[0].temperature);
  EXPECT_FALSE(weather_items[0].icon.IsEmpty());
}

TEST_F(BirchWeatherProviderTest, NoWeatherInfo) {
  auto* birch_model = Shell::Get()->birch_model();

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title);
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature);
  EXPECT_FALSE(weather_items[0].icon.IsEmpty());

  WeatherInfo info2;
  info2.condition_description = "Sunny";
  info2.condition_icon_url = "https://fake-icon-url";
  info2.show_celsius = false;
  info2.temp_f = 73.0f;
  ambient_backend_controller_->SetWeatherInfo(info2);

  base::RunLoop run_loop2;
  birch_model->RequestBirchDataFetch(run_loop2.QuitClosure());
  run_loop2.Run();

  auto& updated_weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, updated_weather_items.size());
  EXPECT_EQ(u"Sunny", updated_weather_items[0].title);
  EXPECT_EQ(u"73\xB0 F", updated_weather_items[0].temperature);
  EXPECT_FALSE(updated_weather_items[0].icon.IsEmpty());
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
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
  run_loop.Run();

  auto& weather_items = birch_model->GetWeatherForTest();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"Cloudy", weather_items[0].title);
  EXPECT_EQ(u"70\xB0 F", weather_items[0].temperature);
  EXPECT_FALSE(weather_items[0].icon.IsEmpty());

  WeatherInfo info2;
  info2.show_celsius = false;
  ambient_backend_controller_->SetWeatherInfo(info2);

  base::RunLoop run_loop2;
  birch_model->RequestBirchDataFetch(run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_TRUE(birch_model->GetWeatherForTest().empty());
}

}  // namespace ash
