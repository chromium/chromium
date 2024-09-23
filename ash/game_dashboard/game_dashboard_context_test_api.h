// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_

#include <string>

#include "ash/game_dashboard/game_dashboard_context.h"
#include "base/memory/raw_ptr.h"

namespace base {
class OneShotTimer;
class RepeatingTimer;
}  // namespace base

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {
class BoxLayoutView;
class Button;
class Label;
class LabelButton;
class View;
class Widget;
}  // namespace views

namespace ash {

class AnchoredNudge;
class FeatureTile;
class GameDashboardBatteryView;
class GameDashboardButton;
class GameDashboardButtonRevealController;
class GameDashboardMainMenuCursorHandler;
class GameDashboardMainMenuView;
class GameDashboardToolbarView;
class IconButton;
class PillButton;
class TimeView;
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
  const base::RepeatingTimer& GetRecordingTimer() const;
  const std::u16string& GetRecordingDuration() const;
  const GameDashboardMainMenuCursorHandler* GetMainMenuCursorHandler() const;

  // Returns the Game Dashboard button widget, button, and title view.
  views::Widget* GetGameDashboardButtonWidget() const;
  GameDashboardButton* GetGameDashboardButton() const;
  float GetGameDashboardButtonCornerRadius() const;
  views::Label* GetGameDashboardButtonTitle() const;
  GameDashboardButtonRevealController* GetGameDashboardButtonRevealController()
      const;
  base::OneShotTimer& GetRevealControllerTopEdgeHoverTimer() const;

  // Returns the main menu widget and all its views.
  views::Widget* GetMainMenuWidget();
  GameDashboardMainMenuView* GetMainMenuView();
  FeatureTile* GetMainMenuGameControlsTile();
  FeatureTile* GetMainMenuToolbarTile();
  FeatureTile* GetMainMenuRecordGameTile();
  FeatureTile* GetMainMenuScreenshotTile();
  const std::u16string& GetMainMenuScreenSizeSubtitle();
  views::Button* GetMainMenuScreenSizeSettingsButton();
  views::Button* GetMainMenuGameControlsDetailsButton();
  PillButton* GetMainMenuGameControlsSetupButton();
  TimeView* GetMainMenuClockView();
  GameDashboardBatteryView* GetMainMenuBatteryView();
  Switch* GetMainMenuGameControlsFeatureSwitch();
  views::LabelButton* GetMainMenuFeedbackButton();
  IconButton* GetMainMenuHelpButton();
  IconButton* GetMainMenuSettingsButton();
  views::BoxLayoutView* GetMainMenuContainer();
  views::BoxLayoutView* GetSettingsContainer();
  IconButton* GetSettingsViewBackButton();
  Switch* GetSettingsViewWelcomeDialogSwitch();

  // Returns the Game Controls setup nudge.
  AnchoredNudge* GetGameControlsSetupNudge();

  // Returns the Game Dashboard welcome dialog widget.
  views::Widget* GetWelcomeDialogWidget();

  // Opens the main menu.
  // Before opening the main menu, verifies that the main menu is closed.
  // After opening the main menu, verifies it opened and waits for the thread to
  // become idle to ensure that all open `GameDashboardMainMenuView`s close.
  void OpenTheMainMenu();

  // Closes the main menu.
  // Before closing the main menu, verifies that the main menu is open.
  // After closing the main menu, verifies is closed and waits for the thread to
  // become idle to ensure that all open `GameDashboardMainMenuView`s close.
  void CloseTheMainMenu();

  // Returns the toolbar widget and all its views.
  views::Widget* GetToolbarWidget();
  GameDashboardToolbarView* GetToolbarView();
  IconButton* GetToolbarGamepadButton();
  IconButton* GetToolbarGameControlsButton();
  IconButton* GetToolbarRecordGameButton();
  IconButton* GetToolbarScreenshotButton();
  bool IsToolbarExpanded();

  // Returns the quadrant that the toolbar is currently placed in.
  GameDashboardToolbarSnapLocation GetToolbarSnapLocation() const;

  // Opens the toolbar.
  // Before opening the toolbar, verifies the main menu is open and the toolbar
  // is closed. After opening the toolbar, verifies it opened.
  void OpenTheToolbar();

  // Places focus on the toolbar without clicking on any buttons.
  void SetFocusOnToolbar();

  // Closes the toolbar.
  // Before closing the toolbar, verifies the main menu widget and toolbar
  // widget are not null. After closing the toolbar, verifies the toolbar widget
  // is null.
  void CloseTheToolbar();

  // Opens the settings view within the main menu view.
  // Before opening the settings view, verifies the main menu widget and main
  // menu container are not null. After opening the settings view, verifies it
  // is open.
  void OpenMainMenuSettings();

  // Closes the settings view and re-opens the main menu within the main menu
  // view. Before closing the settings view, verifies the main menu widget and
  // settings container are not null. After closing the settings view, verifies
  // the settings view is hidden and the main menu is visible.
  void CloseTheSettings();

  // Toggles the Welcome Dialog switch in the settings view.
  // Before toggling the switch, verifies the settings view view is visible.
  // After toggling the switch, verifies the switch state has changed.
  void ToggleWelcomeDialogSettingsSwitch();

  // Verifies the accessibility tree matches the available Game Dashboard
  // related traversable widgets.
  void VerifyAccessibilityTree();

 private:
  // Returns a view from the `GameDashboardMainMenuView` for the given
  // `view_id`.
  views::View* GetMainMenuViewById(int view_id);

  const raw_ptr<GameDashboardContext, DanglingUntriaged> context_;
  const raw_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_TEST_API_H_
