// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_H_
#define ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class AmbientWeatherModelObserver;

// Stores information about the current weather, including an icon representing
// weather conditions (a sun, a cloud, etc.).
class ASH_EXPORT AmbientWeatherModel {
 public:
  AmbientWeatherModel();
  AmbientWeatherModel(const AmbientWeatherModel&) = delete;
  AmbientWeatherModel& operator=(AmbientWeatherModel&) = delete;
  ~AmbientWeatherModel();

  void AddObserver(AmbientWeatherModelObserver* observer);
  void RemoveObserver(AmbientWeatherModelObserver* observer);

  // Updates the weather information and notifies observers.
  void UpdateWeatherInfo(const gfx::ImageSkia& weather_condition_icon,
                         float temperature_fahrenheit,
                         bool show_celsius);

  // Checks if the model has not been fully updated. Currently there's no way to
  // check if the temperature field is a valid value or not.
  bool IsIncomplete() const { return weather_condition_icon_.isNull(); }

  // Returns the cached condition icon. Will return a null image if it has not
  // been set yet.
  const gfx::ImageSkia& weather_condition_icon() const {
    return weather_condition_icon_;
  }

  // Returns the cached temperature value in Fahrenheit.
  float temperature_fahrenheit() const { return temperature_fahrenheit_; }

  // Calculate the temperature in celsius.
  float GetTemperatureInCelsius() const;

  bool show_celsius() const { return show_celsius_; }

 private:
  void NotifyWeatherInfoUpdated();

  // Current weather information.
  gfx::ImageSkia weather_condition_icon_;
  float temperature_fahrenheit_ = 0.0f;
  bool show_celsius_ = false;

  base::ObserverList<AmbientWeatherModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_H_
