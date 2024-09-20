// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_BATTERY_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_BATTERY_VIEW_H_

#include <optional>

#include "ash/system/power/power_status.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// This view is used to display the battery icon utility feature in the Game
// Dashboard.
class ASH_EXPORT GameDashboardBatteryView : public views::ImageView,
                                            public PowerStatus::Observer {
  METADATA_HEADER(GameDashboardBatteryView, views::ImageView)
 public:
  GameDashboardBatteryView();
  GameDashboardBatteryView(const GameDashboardBatteryView&) = delete;
  GameDashboardBatteryView& operator=(const GameDashboardBatteryView&) = delete;
  ~GameDashboardBatteryView() override;

  // views::View:
  void OnThemeChanged() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  // Update image status, visibility, and accessibility strings.`theme_changed`
  // is true when `OnThemeChanged` is called, but is otherwise false.
  void UpdateStatus(bool theme_changed = false);
  // Update battery icon to accurately reflect icon color and power level and
  // status. If `theme_changed` is false and the power level is unchanged, the
  // icon will not update.
  void MaybeUpdateImage(bool theme_changed);

  // Information stored by PowerStatus which pertains to the battery image
  // as a whole.
  std::optional<PowerStatus::BatteryImageInfo> battery_image_info_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_BATTERY_VIEW_H_
