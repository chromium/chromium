// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_

#include "ui/aura/window_observer.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class GameDashboardContext;
class IconButton;

// GameDashboardToolbarView is the movable toolbar that's attached to the game
// window. It contains various quick action tiles for users to access without
// having to open the entire main menu view.
class GameDashboardToolbarView : public views::BoxLayoutView,
                                 public aura::WindowObserver {
 public:
  METADATA_HEADER(GameDashboardToolbarView);

  explicit GameDashboardToolbarView(GameDashboardContext* context);
  GameDashboardToolbarView(const GameDashboardToolbarView&) = delete;
  GameDashboardToolbarView& operator=(const GameDashboardToolbarView) = delete;
  ~GameDashboardToolbarView() override;

 private:
  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class ToolbarViewId : int32_t {
    kGamepadButton = 1,
    kGameControlsButton = 2,
    kScreenRecordButton = 3,
    kScreenshotButton = 4,
  };

  friend class GameDashboardContextTest;

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

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // The topmost `IconButton` in the toolbar's collection, which stays visible
  // in both the expanded and collapsed toolbar states.
  raw_ptr<IconButton, ExperimentalAsh> gamepad_button_;

  // Game Controls toggle button for enabling or disabling the feature.
  raw_ptr<IconButton, ExperimentalAsh> game_controls_button_;

  // The current state indicating if the toolbar view is expanded or collapsed.
  bool is_expanded_ = true;

  const raw_ptr<GameDashboardContext, ExperimentalAsh> context_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
