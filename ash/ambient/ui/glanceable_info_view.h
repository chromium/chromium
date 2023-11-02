// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
#define ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_

#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/ambient/model/ambient_weather_model_observer.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AmbientViewDelegate;
class TimeView;

// Container for displaying a glanceable clock and weather info.
class GlanceableInfoView : public views::View,
                           public AmbientWeatherModelObserver {
 public:
  METADATA_HEADER(GlanceableInfoView);

  GlanceableInfoView(AmbientViewDelegate* delegate,
                     int time_font_size_dip,
                     SkColor time_temperature_font_color);
  GlanceableInfoView(const GlanceableInfoView&) = delete;
  GlanceableInfoView& operator=(const GlanceableInfoView&) = delete;
  ~GlanceableInfoView() override;

  // views::View:
  void OnThemeChanged() override;

  // AmbientWeatherModelObserver:
  void OnWeatherInfoUpdated() override;

  void Show();

 private:
  void InitLayout();

  std::u16string GetTemperatureText() const;

  // View for the time info. Owned by the view hierarchy.
  TimeView* time_view_ = nullptr;

  // Views for weather icon and temperature.
  views::ImageView* weather_condition_icon_ = nullptr;
  views::Label* temperature_ = nullptr;

  // Owned by |AmbientController|.
  AmbientViewDelegate* const delegate_ = nullptr;

  const int time_font_size_dip_;
  const SkColor time_temperature_font_color_;

  base::ScopedObservation<AmbientWeatherModel, AmbientWeatherModelObserver>
      scoped_weather_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
