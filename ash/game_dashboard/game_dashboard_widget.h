// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_WIDGET_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_WIDGET_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace ash {

// GameDashboardWidget is a subclass of widget which overrides the
// `ui::ColorProviderKey` to set Game Dashboard specific color theme.
class ASH_EXPORT GameDashboardWidget : public views::Widget {
 public:
  GameDashboardWidget();
  GameDashboardWidget(const GameDashboardWidget&) = delete;
  GameDashboardWidget& operator=(const GameDashboardWidget&) = delete;
  ~GameDashboardWidget() override;

  // views::Widget:
  ui::ColorProviderKey GetColorProviderKey() const override;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_WIDGET_H_
