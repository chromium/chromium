// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_widget.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"

namespace {

// The base color for all `GameDashboardWidget`s.
constexpr SkColor kWindlightSeedColor = SkColorSetRGB(0x3F, 0x5A, 0xA9);

}  // namespace

namespace ash {

GameDashboardWidget::GameDashboardWidget() = default;

GameDashboardWidget::~GameDashboardWidget() = default;

ui::ColorProviderKey GameDashboardWidget::GetColorProviderKey() const {
  auto key = Widget::GetColorProviderKey();
  key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  key.scheme_variant = ui::ColorProviderKey::SchemeVariant::kTonalSpot;
  key.user_color = kWindlightSeedColor;
  return key;
}

}  // namespace ash
