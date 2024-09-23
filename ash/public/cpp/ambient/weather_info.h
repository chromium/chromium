// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_WEATHER_INFO_H_
#define ASH_PUBLIC_CPP_AMBIENT_WEATHER_INFO_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// WeatherInfo contains the weather information we need for rendering a
// glanceable weather content on Ambient Mode. Corresponding to the
// |backdrop::WeatherInfo| proto.
struct ASH_PUBLIC_EXPORT WeatherInfo {
  WeatherInfo();
  WeatherInfo(const WeatherInfo&);
  WeatherInfo& operator=(const WeatherInfo&);
  ~WeatherInfo();

  // The description of the weather condition.
  std::optional<std::string> condition_description;

  // The url of the weather condition icon image.
  std::optional<std::string> condition_icon_url;

  // Weather temperature in Fahrenheit.
  std::optional<float> temp_f;

  // If the temperature should be displayed in celsius. Conversion must happen
  // before the value in temp_f is displayed.
  bool show_celsius = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_WEATHER_INFO_H_
