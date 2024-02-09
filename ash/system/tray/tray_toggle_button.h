// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_
#define ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_

#include <optional>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// A toggle button configured for the system tray menu's layout. Also gets the
// colors from AshColorProvider.
class ASH_EXPORT TrayToggleButton : public views::ToggleButton {
  METADATA_HEADER(TrayToggleButton, views::ToggleButton)

 public:
  // Creates a button that invokes `callback` when pressed. Sets the accessible
  // name to the string with resource id `accessible_name_id`, unless that
  // parameter is nullopt. If `use_empty_border` is false, adds an empty border
  // to pad the toggle to 68x48 pixels (for legacy status area). If
  // `use_empty_border` is true, the toggle button is just the size of the
  // underlying button.
  TrayToggleButton(PressedCallback callback,
                   std::optional<int> accessible_name_id,
                   bool use_empty_border = false);
  TrayToggleButton(const TrayToggleButton&) = delete;
  TrayToggleButton& operator=(const TrayToggleButton&) = delete;
  ~TrayToggleButton() override = default;

  // views::ToggleButton:
  void OnThemeChanged() override;
  void NotifyClick(const ui::Event& event) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_TOGGLE_BUTTON_H_
