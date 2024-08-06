// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_
#define ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/public/cpp/ambient/weather_info.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

class BirchModel;

class ASH_EXPORT BirchWeatherProvider : public BirchDataProvider {
 public:
  explicit BirchWeatherProvider(BirchModel* birch_model);
  BirchWeatherProvider(const BirchWeatherProvider&) = delete;
  BirchWeatherProvider& operator=(const BirchWeatherProvider&) = delete;
  ~BirchWeatherProvider() override;

  // Called from birch model to request weather information to be displayed in
  // UI.
  void RequestBirchDataFetch() override;

  void ResetCacheForTest();

 private:
  // Performs the weather fetch via the ambient controller.
  void FetchWeather();

  // Called in response to a weather info request. It initiates icon fetch from
  // the URL provided in the weather info.
  void OnWeatherInfoFetched(const std::optional<WeatherInfo>& weather_info);

  // Callback to weather info icon request. It will update birch model with the
  // fetched weather info (including the downloaded weather icon).
  void OnWeatherConditionIconDownloaded(
      const std::string& condition_icon_url,
      const std::u16string& weather_description,
      float temp_f,
      const gfx::ImageSkia& icon);

  // Adds the weather item to the birch model.
  void AddItemToBirchModel(const std::u16string& weather_description,
                           float temp_f,
                           const std::string& icon_url);

  const raw_ptr<BirchModel> birch_model_;
  bool is_fetching_ = false;

  // Support for caching the last fetch.
  base::Time last_fetch_time_;
  std::optional<WeatherInfo> last_weather_info_;

  base::WeakPtrFactory<BirchWeatherProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_
