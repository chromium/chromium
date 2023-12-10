// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientWeatherModel;
struct WeatherInfo;

// Handles fetching weather information from the backdrop server, including the
// weather condition icon image (a sun, a cloud, etc.). Owns the data model that
// caches the current weather info.
class ASH_EXPORT AmbientWeatherController
    : public SimpleGeolocationProvider::Observer {
 public:
  // Causes AmbientWeatherController to periodically refresh the weather info
  // in the model for as long as this object is alive. The latest weather is
  // also fetched immediately upon construction if there are currently no
  // active `ScopedRefresher`s.
  class ScopedRefresher {
   public:
    ScopedRefresher(const ScopedRefresher&) = delete;
    ScopedRefresher& operator=(const ScopedRefresher&) = delete;
    ~ScopedRefresher();

   private:
    friend class AmbientWeatherController;

    explicit ScopedRefresher(AmbientWeatherController* controller);

    const raw_ptr<AmbientWeatherController> controller_;
  };

  explicit AmbientWeatherController(
      SimpleGeolocationProvider* const location_permission_provider);
  AmbientWeatherController(const AmbientWeatherController&) = delete;
  AmbientWeatherController& operator=(const AmbientWeatherController&) = delete;
  ~AmbientWeatherController() override;

  // SimpleGeolocationProvider::Observer:
  void OnGeolocationPermissionChanged(bool enabled) override;

  // Always returns non-null.
  std::unique_ptr<ScopedRefresher> CreateScopedRefresher();

  AmbientWeatherModel* weather_model() { return weather_model_.get(); }

 private:
  // Triggers a fetch of weather information and a download of the appropriate
  // weather condition icon.
  void FetchWeather();

  void StartDownloadingWeatherConditionIcon(
      const std::optional<WeatherInfo>& weather_info);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(float temp_f,
                                        bool show_celsius,
                                        const gfx::ImageSkia& icon);

  void OnScopedRefresherDestroyed();

  const raw_ptr<SimpleGeolocationProvider> location_permission_provider_ =
      nullptr;

  std::unique_ptr<AmbientWeatherModel> weather_model_;

  int num_active_scoped_refreshers_ = 0;

  base::RepeatingTimer weather_refresh_timer_;

  base::WeakPtrFactory<AmbientWeatherController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_WEATHER_CONTROLLER_H_
