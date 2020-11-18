// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
#define ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

namespace tray {
class TimeView;
}

class AmbientViewDelegate;

// Container for displaying a glanceable clock and weather info.
class GlanceableInfoView : public views::View,
                           public AmbientBackendModelObserver {
 public:
  explicit GlanceableInfoView(AmbientViewDelegate* delegate);
  GlanceableInfoView(const GlanceableInfoView&) = delete;
  GlanceableInfoView& operator=(const GlanceableInfoView&) = delete;
  ~GlanceableInfoView() override;

  // views::View:
  const char* GetClassName() const override;

  // AmbientBackendModelObserver:
  void OnWeatherInfoUpdated() override;
  void OnImagesChanged() override {}

  void Show();

 private:
  void InitLayout();

  base::string16 GetTemperatureText() const;

  // View for the time info. Owned by the view hierarchy.
  ash::tray::TimeView* time_view_ = nullptr;

  // Views for weather icon and temperature.
  views::ImageView* weather_condition_icon_ = nullptr;
  views::Label* temperature_ = nullptr;

  // Owned by |AmbientController|.
  AmbientViewDelegate* const delegate_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
