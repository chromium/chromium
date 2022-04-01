// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_H_

#include <memory>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_item_view.h"

namespace ash {

class PowerTrayView : public TrayItemView,
                      public PowerStatus::Observer,
                      public SessionObserver {
 public:
  explicit PowerTrayView(Shelf* shelf);

  PowerTrayView(const PowerTrayView&) = delete;
  PowerTrayView& operator=(const PowerTrayView&) = delete;

  ~PowerTrayView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  void UpdateStatus();
  void UpdateImage(bool icon_color_changed);

  std::u16string accessible_name_;
  std::u16string tooltip_;
  absl::optional<PowerStatus::BatteryImageInfo> info_;
  session_manager::SessionState session_state_ =
      session_manager::SessionState::UNKNOWN;
  ScopedSessionObserver session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_H_
