// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_
#define ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_

#include <optional>

#include "ash/birch/birch_client.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

class BirchModel;

struct WeatherInfo;

class BirchWeatherProvider : public BirchClient {
 public:
  explicit BirchWeatherProvider(BirchModel* birch_model);
  BirchWeatherProvider(const BirchWeatherProvider&) = delete;
  BirchWeatherProvider& operator=(const BirchWeatherProvider&) = delete;
  ~BirchWeatherProvider() override;

  // Called from birch model to request weather information to be displayed in
  // UI.
  void RequestBirchDataFetch() override;

 private:
  // Called in response to a weather info request. It initiates icon fetch from
  // the URL provided in the weather info.
  void OnWeatherInfoFetched(const std::optional<WeatherInfo>& weather_info);

  // Callback to weather info icon request. It will update birch model with the
  // fetched weather info (including the downloaded weather icon).
  void OnWeatherConditionIconDownloaded(
      const std::u16string& weather_description,
      float temp_f,
      bool show_celsius,
      const gfx::ImageSkia& icon);

  const raw_ptr<BirchModel> birch_model_;

  base::WeakPtrFactory<BirchWeatherProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_WEATHER_PROVIDER_H_
