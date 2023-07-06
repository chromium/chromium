// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_

#include "ui/views/layout/box_layout_view.h"

namespace ash {

// GameDashboardToolbarView is the movable toolbar that's attached to the game
// window. It contains various quick action tiles for users to access without
// having to open the entire main menu view.
class GameDashboardToolbarView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(GameDashboardToolbarView);

  GameDashboardToolbarView();
  GameDashboardToolbarView(const GameDashboardToolbarView&) = delete;
  GameDashboardToolbarView& operator=(const GameDashboardToolbarView) = delete;
  ~GameDashboardToolbarView() override;

 private:
  // Callbacks for the tiles and buttons in the toolbar view.
  // Expands or collapses the toolbar.
  void OnGamepadButtonPressed();
  // Starts or stops recording the game window.
  void OnRecordButtonPressed();
  // Takes a screenshot of the game window.
  void OnScreenshotButtonPressed();

  // Adds a list of shortcut tiles to the toolbar view.
  void AddShortcutTiles();
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_TOOLBAR_VIEW_H_
