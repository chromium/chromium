// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_POWER_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_POWER_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace ash {
// The power button that lives in the `QuickSettingsView` footer. It is with a
// `PowerButtonMenuController` which can show the power menu when on
// `OnButtonActivated`.
class ASH_EXPORT PowerButton : public IconButton {
 public:
  PowerButton();
  PowerButton(const PowerButton&) = delete;
  PowerButton& operator=(const PowerButton&) = delete;
  ~PowerButton() override;

 private:
  friend class PowerButtonTest;
  friend class QuickSettingsFooterTest;

  // This class is the context menu controller used by `PowerButton` in the
  // `QuickSettingsFooter`, responsible for building, running the menu and
  // executing the commands.
  class MenuController;

  // Shows the context menu by `MenuController`. This method passed in to the
  // base `IconButton` as the `OnPressedCallback`.
  void OnButtonActivated(const ui::Event& event);

  // Getters for testing.
  views::MenuItemView* GetMenuViewForTesting();
  views::MenuRunner* GetMenuRunnerForTesting();

  // The context menu, which will be set as the controller to show the power
  // button menu view.
  std::unique_ptr<MenuController> context_menu_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_POWER_BUTTON_H_
