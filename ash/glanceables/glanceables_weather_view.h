// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_WEATHER_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_WEATHER_VIEW_H_

#include "ash/ambient/model/ambient_weather_model_observer.h"
#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// Glanceables screen view for weather information. Shows an icon for the
// current conditions (e.g. a sun or a cloud) and the current temperature.
class ASH_EXPORT GlanceablesWeatherView : public views::View,
                                          public AmbientWeatherModelObserver {
 public:
  GlanceablesWeatherView();
  GlanceablesWeatherView(const GlanceablesWeatherView&) = delete;
  GlanceablesWeatherView& operator=(const GlanceablesWeatherView&) = delete;
  ~GlanceablesWeatherView() override;

  // AmbientWeatherModelObserver:
  void OnWeatherInfoUpdated() override;

 private:
  friend class GlanceablesTest;

  views::ImageView* icon_ = nullptr;
  views::Label* temperature_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_WEATHER_VIEW_H_
