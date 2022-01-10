// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_
#define ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_

#include "ui/views/controls/button/toggle_button.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// A toggle button configured for the system tray menu's layout. Also gets the
// colors from AshColorProvider.
class TrayToggleButton : public views::ToggleButton {
 public:
  TrayToggleButton(PressedCallback callback, int accessible_name_id);
  TrayToggleButton(const TrayToggleButton&) = delete;
  TrayToggleButton& operator=(const TrayToggleButton&) = delete;
  ~TrayToggleButton() override = default;

  // views::ToggleButton:
  void OnThemeChanged() override;
  void NotifyClick(const ui::Event& event) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_
