// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_H_

#include <memory>

#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class PowerTrayView : public TrayItemView, public PowerStatus::Observer {
  METADATA_HEADER(PowerTrayView, TrayItemView)

 public:
  explicit PowerTrayView(Shelf* shelf);

  PowerTrayView(const PowerTrayView&) = delete;
  PowerTrayView& operator=(const PowerTrayView&) = delete;

  ~PowerTrayView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  void UpdateStatus(bool icon_color_changed);
  void UpdateImage(bool icon_color_changed);
  void UpdateAccessibleName();

  std::u16string tooltip_;
  std::optional<PowerStatus::BatteryImageInfo> info_;
  bool previous_battery_saver_state_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_H_
