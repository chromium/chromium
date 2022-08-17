// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_view.h"

#include <memory>

#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/welcome_label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

GlanceablesView::GlanceablesView() {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  welcome_label_ = AddChildView(std::make_unique<GlanceablesWelcomeLabel>());

  weather_view_ = AddChildView(std::make_unique<GlanceablesWeatherView>());
}

GlanceablesView::~GlanceablesView() = default;

}  // namespace ash
