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
}  // namespace views

namespace ash {

// The power button that lives in the `QuickSettingsView` footer. The
// `background_view_` will change its corner radii and a power button
// menu will pop up at the same time when it's active.
class ASH_EXPORT PowerButton : public views::View {
 public:
  PowerButton();
  PowerButton(const PowerButton&) = delete;
  PowerButton& operator=(const PowerButton&) = delete;
  ~PowerButton() override;

  // If the context mune is currently open.
  bool IsMenuShowing();

 private:
  friend class PowerButtonTest;
  friend class QuickSettingsFooterTest;

  // This class is the context menu controller used by `PowerButton` in the
  // `QuickSettingsFooter`, responsible for building, running the menu and
  // executing the commands.
  class MenuController;

  // views::View:
  void OnThemeChanged() override;

  // Updates the shape (rounded corner radii) and color of this view. Also
  // re-paints the focus ring.
  void UpdateView();

  // Updates the rounded corner radii based on the current `PowerButton` state.
  void UpdateRoundedCorners();

  // Shows the context menu by `MenuController`. This method passed in to the
  // base `IconButton` as the `OnPressedCallback`.
  void OnButtonActivated(const ui::Event& event);

  // Getter of the `MenuItemView` for testing.
  views::MenuItemView* GetMenuViewForTesting();

  // Owned by views hierarchy.
  views::View* background_view_ = nullptr;
  IconButton* button_content_ = nullptr;

  // The context menu, which will be set as the controller to show the power
  // button menu view.
  std::unique_ptr<MenuController> context_menu_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_POWER_BUTTON_H_
