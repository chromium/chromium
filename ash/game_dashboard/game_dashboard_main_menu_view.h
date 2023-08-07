// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ash {

class FeatureTile;
class GameDashboardContext;
class PillButton;
class Switch;

// GameDashboardMainMenuView is the expanded menu view attached to the game
// dashboard button.
class ASH_EXPORT GameDashboardMainMenuView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(GameDashboardMainMenuView);

  explicit GameDashboardMainMenuView(GameDashboardContext* context);

  GameDashboardMainMenuView(views::Widget* main_menu_button_widget,
                            aura::Window* game_window);
  GameDashboardMainMenuView(const GameDashboardMainMenuView&) = delete;
  GameDashboardMainMenuView& operator=(const GameDashboardMainMenuView) =
      delete;
  ~GameDashboardMainMenuView() override;

  // Updates `record_game_tile_` to the stop button. Called when
  // `CaptureModeController` has started a recording session. If
  // `is_recording_game_window` is true, then the `context_`'s game_window is
  // being recorded.
  void OnRecordingStarted(bool is_recording_game_window);

  // Updates `record_game_tile_` to the default state. Called when
  // `CaptureModeController` has ended a recording session or was aborted.
  void OnRecordingEnded();

 private:
  friend class GameDashboardContextTestApi;

  class FeatureDetailsRow;

  // Callbacks for the tiles and buttons in the main menu view.
  // Handles showing and hiding the toolbar.
  void OnToolbarTilePressed();
  // Handles toggling the game recording.
  void OnRecordGameTilePressed();
  // Handles taking a screenshot of the game window when pressed.
  void OnScreenshotTilePressed();

  // Handles functions for Game Controls buttons.
  void OnGameControlsTilePressed();
  void OnGameControlsDetailsPressed();
  void OnGameControlsSetUpButtonPressed();
  void OnGameControlsHintSwitchButtonPressed();

  // Handles when the Screen Size Settings is pressed.
  void OnScreenSizeSettingsButtonPressed();
  // Opens the feedback form with preset information.
  void OnFeedbackButtonPressed();
  // Opens the help center for more info about Game Dashboard.
  void OnHelpButtonPressed();
  // Opens up the Game Dashboard Settings.
  void OnSettingsButtonPressed();

  // Adds a row of shortcut tiles to the main menu view for users to quickly
  // access common functionality.
  void AddShortcutTilesRow();

  // Adds feature details rows, for example, including Game Controls or window
  // size.
  void AddFeatureDetailsRows();

  // Adds Game Controls feature tile in `container` if it is the ARC game window
  // and Game Controls is available.
  void MaybeAddGameControlsTile(views::View* container);

  // Adds menu controls row for Game Controls.
  void MaybeAddGameControlsDetailsRow(views::View* container);

  // Adds a row to access a settings page controlling the screen size if the
  // given game window is an ARC app.
  void MaybeAddScreenSizeSettingsRow(views::View* container);

  // Adds the dashboard cluster (containing feedback, settings, and help
  // buttons) to the Game Controls tile view.
  void AddUtilityClusterRow();

  // Enables Game Controls edit mode.
  void EnableGameControlsEditMode();

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Updates the `record_game_tile_` UI. If `is_recording_game_window` is
  // true, then the tile will change to a stop button, otherwise it will show
  // the default UI.
  void UpdateRecordGameTile(bool is_recording_game_window);

  // Allows this class to access `GameDashboardContext` owned functions/objects.
  const raw_ptr<GameDashboardContext, ExperimentalAsh> context_;

  // Shortcut Tiles:
  // Toolbar button to toggle the `GameDashboardToolbarView`.
  raw_ptr<FeatureTile> toolbar_tile_ = nullptr;

  // Game controls button to toggle the Game Controls UI visibility.
  raw_ptr<FeatureTile> game_controls_tile_ = nullptr;

  // Record game button to start recording the game window. This will open the
  // screen capture tool, allowing the user to select recording options.
  raw_ptr<FeatureTile> record_game_tile_ = nullptr;

  // Game Controls details:
  // Feature row to configure Game Controls.
  raw_ptr<FeatureDetailsRow> game_controls_details_ = nullptr;

  // Setup button to configure Game Controls for the current game window.
  raw_ptr<PillButton> game_controls_setup_button_ = nullptr;

  // Hint button to toggle the Game Controls hint UI.
  raw_ptr<Switch> game_controls_hint_switch_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
