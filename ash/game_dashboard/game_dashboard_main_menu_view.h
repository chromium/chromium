// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/unified/feature_tile.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace ash {

class AnchoredNudge;
class GameDashboardBatteryView;
class GameDashboardContext;
class IconButton;
class PillButton;
class Switch;
class TimeView;

// GameDashboardMainMenuView is the expanded menu view attached to the game
// dashboard button.
class ASH_EXPORT GameDashboardMainMenuView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(GameDashboardMainMenuView, views::BubbleDialogDelegateView)

 public:
  explicit GameDashboardMainMenuView(GameDashboardContext* context);

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

  // Updates the `record_game_tile_`'s sub-label with `duration`, showing the
  // recording duration.
  void UpdateRecordingDuration(const std::u16string& duration);

  // Updates the `game_controls_tile_` states, sub-label and tooltip text.
  void UpdateGameControlsTile();

 private:
  friend class GameDashboardContextTestApi;

  class ScreenSizeRow;
  class GameControlsDetailsRow;

  // Callbacks for the tiles and buttons in the main menu view.
  // Handles showing and hiding the toolbar.
  void OnToolbarTilePressed();
  // Handles toggling the game recording.
  void OnRecordGameTilePressed();
  // Handles taking a screenshot of the game window when pressed.
  void OnScreenshotTilePressed();

  // Callbacks for the buttons in the settings view.
  // Handles going back to the main menu view when pressed.
  void OnSettingsBackButtonPressed();
  // Handles toggling the welcome dialog preference and updating the Switch
  // state.
  void OnWelcomeDialogSwitchPressed();

  // Handles functions for Game Controls buttons.
  void OnGameControlsTilePressed();

  // Handles when the Screen Size Settings is pressed.
  void OnScreenSizeSettingsButtonPressed();
  // Opens the feedback form with preset information.
  void OnFeedbackButtonPressed();
  // Opens the help center for more info about Game Dashboard.
  void OnHelpButtonPressed();
  // Opens up the Game Dashboard Settings.
  void OnSettingsButtonPressed();

  // Creates the `main_menu_container_` and adds all rows of views pertaining to
  // the main menu view to it.
  void AddMainMenuViews();

  // Adds a row of shortcut tiles to the `main_menu_container_` for users to
  // quickly access common functionality.
  void AddShortcutTilesRow();

  // Adds feature details rows, for example, including Game Controls or window
  // size to the `main_menu_container_`.
  void MaybeAddArcFeatureRows();

  // Adds Game Controls feature tile of type `tile_type` to a specified
  // `container`.
  void AddGameControlsTile(views::View* container,
                           FeatureTile::TileType tile_type);

  // Adds menu controls row for Game Controls.
  void AddGameControlsDetailsRow(views::View* container,
                                 const gfx::RoundedCornersF& row_corners);

  // Adds Record Game feature tile of type `tile_type` to a specified
  // `container`.
  void AddRecordGameTile(views::View* container,
                         FeatureTile::TileType tile_type);

  // Adds a row to access a settings page controlling the screen size if the
  // given game window is an ARC app.
  void AddScreenSizeSettingsRow(views::View* container);

  // Adds the dashboard cluster (containing feedback, settings, and help
  // buttons) to the `main_menu_container_`.
  void AddUtilityClusterRow();

  // Adds utility features to the utility `container` in the Main Menu.
  void AddUtilityFeatureViews(views::View* container);

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Updates the `record_game_tile_` UI. If `is_recording_game_window` is
  // true, then the tile will change to a stop button, otherwise it will show
  // the default UI.
  void UpdateRecordGameTile(bool is_recording_game_window);

  // Creates the `settings_view_container_` and adds all rows pertaining to the
  // settings view to it.
  void AddSettingsViews();

  // Adds a row displaying the title and back button.
  void AddSettingsTitleRow();

  // Adds a row displaying the welcome dialog setting.
  void AddWelcomeDialogSettingsRow();

  // Updates the accessible name for the `welcome_dialog_settings_switch_`.
  void OnWelcomeDialogSwitchStateChanged(bool is_enabled);

  // Gets UI elements from Game Controls details row.
  PillButton* GetGameControlsSetupButton();
  Switch* GetGameControlsFeatureSwitch();

  // For test to access the nudge ID in the anonymous namespace.
  AnchoredNudge* GetGameControlsSetupNudgeForTesting();

  // Returns the screen size row sub-label. If the screen size row or the
  // sub-label aren't available, returns null. Visible for testing.
  const views::Label* GetScreenSizeRowSubtitle();

  // views::Views:
  void OnThemeChanged() override;

  // Allows this class to access `GameDashboardContext` owned functions/objects.
  const raw_ptr<GameDashboardContext> context_;

  // Container holding all views displayed in the main menu view.
  raw_ptr<views::BoxLayoutView> main_menu_container_ = nullptr;

  // Container holding all views displayed in the settings view.
  raw_ptr<views::BoxLayoutView> settings_view_container_ = nullptr;

  // Shortcut Tiles:
  // Toolbar button to toggle the `GameDashboardToolbarView`.
  raw_ptr<FeatureTile> toolbar_tile_ = nullptr;

  // Game controls button to toggle the Game Controls UI hint.
  raw_ptr<FeatureTile> game_controls_tile_ = nullptr;

  // Record game button to start recording the game window. This will open the
  // screen capture tool, allowing the user to select recording options.
  raw_ptr<FeatureTile> record_game_tile_ = nullptr;

  // Screen Size Settings detail row. Visible for testing.
  raw_ptr<ScreenSizeRow> screen_size_row_ = nullptr;

  // Game Controls details row to configure Game Controls.
  raw_ptr<GameControlsDetailsRow> game_controls_details_ = nullptr;

  // The `Switch` representing toggling the welcome dialog within the settings.
  raw_ptr<Switch> welcome_dialog_settings_switch_ = nullptr;

  // Back button in the `settings_view_container_`. Visible for testing.
  raw_ptr<IconButton> settings_view_back_button_ = nullptr;

  // The clock displayed in the utility cluster row. Visible for testing.
  raw_ptr<TimeView> clock_view_ = nullptr;

  // The battery displayed in the utility cluster row. Visible for testing.
  raw_ptr<GameDashboardBatteryView> battery_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
