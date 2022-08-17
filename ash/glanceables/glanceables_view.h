// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class GlanceablesWeatherView;
class GlanceablesWelcomeLabel;

// Container view for the "welcome back" glanceables screen shown on login.
class ASH_EXPORT GlanceablesView : public views::View {
 public:
  GlanceablesView();
  GlanceablesView(const GlanceablesView&) = delete;
  GlanceablesView& operator=(const GlanceablesView&) = delete;
  ~GlanceablesView() override;

  GlanceablesWelcomeLabel* welcome_label_for_test() { return welcome_label_; }
  GlanceablesWeatherView* weather_view_for_test() { return weather_view_; }

 private:
  GlanceablesWelcomeLabel* welcome_label_ = nullptr;
  GlanceablesWeatherView* weather_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_VIEW_H_
