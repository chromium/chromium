// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class GameDashboardContext;
class IconButton;
class ToolbarDragHandler;
class SystemShadow;

// GameDashboardToolbarView is the movable toolbar that's attached to the game
// window. It contains various quick action tiles for users to access without
// having to open the entire main menu view.
class ASH_EXPORT GameDashboardToolbarView : public views::BoxLayoutView {
  METADATA_HEADER(GameDashboardToolbarView, views::BoxLayoutView)

 public:
  explicit GameDashboardToolbarView(GameDashboardContext* context);
  GameDashboardToolbarView(const GameDashboardToolbarView&) = delete;
  GameDashboardToolbarView& operator=(const GameDashboardToolbarView) = delete;
  ~GameDashboardToolbarView() override;

  // Updates `record_game_button_` to the stop button. Called when
  // `CaptureModeController` has started a recording session. If
  // `is_recording_game_window` is true, then the `context_`'s game_window is
  // being recorded.
  void OnRecordingStarted(bool is_recording_game_window);

  // Updates `record_game_button_` to the default state. Called when
  // `CaptureModeController` has ended a recording session or was aborted.
  void OnRecordingEnded();

  // Handles repositioning the toolbar view within the game window.
  void RepositionToolbar(const gfx::Vector2d& offset);

  // Handles completion of the toolbar movement.
  void EndDraggingToolbar(const gfx::Vector2d& offset);

  // Updates this view's widget visibility. If it is visible, updates
  // game_controls_button_'s state, and the tooltip text according to flags.
  void UpdateViewForGameControls(ArcGameControlsFlag flags);

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;

 private:
  friend class GameDashboardContextTestApi;

  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class ToolbarViewId : int32_t {
    kGamepadButton = 1,
    kGameControlsButton = 2,
    kScreenRecordButton = 3,
    kScreenshotButton = 4,
  };

  // Callbacks for the tiles and buttons in the toolbar view.
  // Expands or collapses the toolbar by iterating through the toolbar's
  // children and updating their visibility. The `gamepad_button_` is always
  // visible since it expands/collapses the toolbar.
  void OnGamepadButtonPressed();
  // Enables or disables Game Controls.
  void OnGameControlsButtonPressed();
  // Starts or stops recording the game window.
  void OnRecordButtonPressed();
  // Takes a screenshot of the game window.
  void OnScreenshotButtonPressed();

  // Adds a list of shortcut tiles to the toolbar view.
  void AddShortcutTiles();
  // Adds Game Controls button if needed.
  void MayAddGameControlsTile();

  // Updates the `record_game_button_` UI. If `is_recording_game_window` is
  // true, then the button will change to a stop button, otherwise it will show
  // the default UI.
  void UpdateRecordGameButton(bool is_recording_game_window);

  // Updates the 'gamepad_button_' tooltip text. If the toolbar is collapsed,
  // the tooltip text will say "Open toolbar" and if the toolbar is expanded,
  // the tooltip text will say "Close toolbar".
  void UpdateGamepadButtonTooltipText();

  // The topmost `IconButton` in the toolbar's collection, which stays visible
  // in both the expanded and collapsed toolbar states.
  raw_ptr<IconButton> gamepad_button_ = nullptr;

  // Game Controls toggle button for enabling or disabling the feature.
  raw_ptr<IconButton> game_controls_button_ = nullptr;

  // Record game button to start recording the game window, skipping the
  // countdown timer and preset screen capture options.
  raw_ptr<IconButton> record_game_button_ = nullptr;

  // The current state indicating if the toolbar view is expanded or collapsed.
  bool is_expanded_ = true;

  const raw_ptr<GameDashboardContext> context_;

  // Handles all dragging logic for the toolbar.
  std::unique_ptr<ToolbarDragHandler> drag_handler_;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
