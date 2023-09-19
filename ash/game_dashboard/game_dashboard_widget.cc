// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_widget.h"

#include "ui/color/color_provider_key.h"

namespace ash {

GameDashboardWidget::GameDashboardWidget() = default;

GameDashboardWidget::~GameDashboardWidget() = default;

ui::ColorProviderKey GameDashboardWidget::GetColorProviderKey() const {
  auto key = Widget::GetColorProviderKey();
  key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  return key;
}

}  // namespace ash
