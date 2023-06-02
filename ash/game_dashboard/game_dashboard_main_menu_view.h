// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ash {

// GameDashboardMainMenuView is the expanded menu view attached to the game
// dashboard button.
class GameDashboardMainMenuView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(GameDashboardMainMenuView);

  explicit GameDashboardMainMenuView(views::Widget* main_menu_button_widget);
  GameDashboardMainMenuView(const GameDashboardMainMenuView&) = delete;
  GameDashboardMainMenuView& operator=(const GameDashboardMainMenuView) =
      delete;
  ~GameDashboardMainMenuView() override;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_VIEW_H_
