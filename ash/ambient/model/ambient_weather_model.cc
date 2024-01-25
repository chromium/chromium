// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_weather_model.h"

#include "ash/ambient/model/ambient_weather_model_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

AmbientWeatherModel::AmbientWeatherModel() = default;

AmbientWeatherModel::~AmbientWeatherModel() = default;

void AmbientWeatherModel::AddObserver(AmbientWeatherModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientWeatherModel::RemoveObserver(
    AmbientWeatherModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AmbientWeatherModel::UpdateWeatherInfo(
    const gfx::ImageSkia& weather_condition_icon,
    float temperature_fahrenheit,
    bool show_celsius) {
  weather_condition_icon_ = weather_condition_icon;
  temperature_fahrenheit_ = temperature_fahrenheit;
  show_celsius_ = show_celsius;

  NotifyWeatherInfoUpdated();
}

float AmbientWeatherModel::GetTemperatureInCelsius() const {
  return (temperature_fahrenheit_ - 32) * 5 / 9;
}

void AmbientWeatherModel::NotifyWeatherInfoUpdated() {
  for (auto& observer : observers_)
    observer.OnWeatherInfoUpdated();
}

}  // namespace ash
