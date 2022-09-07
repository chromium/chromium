// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientWeatherModel;
struct WeatherInfo;

// Handles fetching weather information from the backdrop server, including the
// weather condition icon image (a sun, a cloud, etc.). Owns the data model that
// caches the current weather info.
class ASH_EXPORT AmbientWeatherController {
 public:
  AmbientWeatherController();
  AmbientWeatherController(const AmbientWeatherController&) = delete;
  AmbientWeatherController& operator=(const AmbientWeatherController&) = delete;
  ~AmbientWeatherController();

  // Triggers a fetch of weather information and a download of the appropriate
  // weather condition icon.
  void FetchWeather();

  AmbientWeatherModel* weather_model() { return weather_model_.get(); }

 private:
  void StartDownloadingWeatherConditionIcon(
      const absl::optional<WeatherInfo>& weather_info);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(float temp_f,
                                        bool show_celsius,
                                        const gfx::ImageSkia& icon);

  std::unique_ptr<AmbientWeatherModel> weather_model_;

  base::WeakPtrFactory<AmbientWeatherController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_
