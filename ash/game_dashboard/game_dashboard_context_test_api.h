// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_

#include "ash/game_dashboard/game_dashboard_context.h"
#include "base/memory/raw_ptr.h"

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {
class Button;
class LabelButton;
class View;
class Widget;
}  // namespace views

namespace ash {

class FeatureTile;
class GameDashboardMainMenuView;
class GameDashboardToolbarView;
class GameDashboardWidget;
class IconButton;
class PillButton;
class Switch;

// Wrapper for `GameDashboardContext` that exposes its internals to test
// functions.
class GameDashboardContextTestApi {
 public:
  GameDashboardContextTestApi(GameDashboardContext* context,
                              ui::test::EventGenerator* event_generator);
  GameDashboardContextTestApi(GameDashboardContextTestApi&) = delete;
  GameDashboardContextTestApi& operator=(GameDashboardContextTestApi&) = delete;
  ~GameDashboardContextTestApi() = default;

  GameDashboardContext* context() { return context_; }

  // Returns the main menu button widget and button.
  GameDashboardWidget* GetMainMenuButtonWidget();
  PillButton* GetMainMenuButton();

  // Returns the main menu widget and all its views.
  views::Widget* GetMainMenuWidget();
  GameDashboardMainMenuView* GetMainMenuView();
  FeatureTile* GetMainMenuGameControlsTile();
  FeatureTile* GetMainMenuToolbarTile();
  FeatureTile* GetMainMenuRecordGameTile();
  FeatureTile* GetMainMenuScreenshotTile();
  views::Button* GetMainMenuScreenSizeSettingsButton();
  views::Button* GetMainMenuGameControlsDetailsButton();
  PillButton* GetMainMenuGameControlsSetupButton();
  Switch* GetMainMenuGameControlsHintSwitch();
  views::LabelButton* GetMainMenuFeedbackButton();
  IconButton* GetMainMenuHelpButton();
  IconButton* GetMainMenuSettingsButton();

  // Opens the main menu.
  // Before opening the main menu, verifies that the main menu is closed.
  // After opening the main menu, verifies it opened.
  void OpenTheMainMenu();

  // Closes the main menu.
  // Before closing the main menu, verifies that the main menu is open.
  // After closing the main menu, verifies is closed.
  void CloseTheMainMenu();

  // Returns the toolbar widget and all its views.
  GameDashboardWidget* GetToolbarWidget();
  GameDashboardToolbarView* GetToolbarView();
  IconButton* GetToolbarGamepadButton();
  IconButton* GetToolbarGameControlsButton();
  IconButton* GetToolbarRecordGameButton();
  IconButton* GetToolbarScreenshotButton();

  // Returns the quadrant that the toolbar is currently placed in.
  GameDashboardContext::ToolbarSnapLocation GetToolbarSnapLocation() const;

  // Opens the toolbar.
  // Before opening the toolbar, verifies the main menu is open and the toolbar
  // is closed. After opening the toolbar, verifies it opened.
  void OpenTheToolbar();

  // Closes the toolbar.
  // Before closing the toolbar, verifies the main menu widget and toolbar
  // widget are not null. After closing the toolbar, verifies the toolbar widget
  // is null.
  void CloseTheToolbar();

 private:
  // Returns a view from the `GameDashboardMainMenuView` for the given
  // `view_id`.
  views::View* GetMainMenuViewById(int view_id);

  const raw_ptr<GameDashboardContext, ExperimentalAsh> context_;
  const raw_ptr<ui::test::EventGenerator, ExperimentalAsh> event_generator_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_
