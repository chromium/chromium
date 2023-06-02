// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// This class manages Game Dashboard related UI for a given `aura::Window`, and
// its instance is managed by the `GameDashboardController`.
class GameDashboardContext {
 public:
  explicit GameDashboardContext(aura::Window* game_window);
  GameDashboardContext(const GameDashboardContext&) = delete;
  GameDashboardContext& operator=(const GameDashboardContext&) = delete;
  ~GameDashboardContext();

  // Called by `GameDashboardController` when the game window bounds change.
  void OnWindowBoundsChanged();

  // Toggles the main menu, called only by the accelerator, or hides the menu
  // if it is already shown.
  void ToggleMainMenu();

 private:
  friend class GameDashboardContextTest;

  // Creates a main menu button widget and adds it as a sibling of the game
  // window.
  void CreateAndAddMainMenuButtonWidget();

  // Updates the main menu button widget's bounds and location, relative to the
  // `game_window_`.
  void UpdateMainMenuButtonWidgetBounds();

  // Called when the button in the `main_menu_button_widget_` is pressed, and
  // toggles the main menu.
  void OnMainMenuButtonPressed();

  const raw_ptr<aura::Window, ExperimentalAsh> game_window_;

  // Main menu button widget for the Game Dashboard.
  std::unique_ptr<views::Widget> main_menu_button_widget_;

  // Expanded main menu for the Game Dashboard.
  views::UniqueWidgetPtr main_menu_widget_;

  base::WeakPtrFactory<GameDashboardContext> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
