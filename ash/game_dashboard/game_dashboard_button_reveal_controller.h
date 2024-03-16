// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

class GameDashboardContext;

// GameDashboardButtonRevealController is responsible for determining when to
// hide/show the GameDashboardButton when the window is in fullscreen.
class GameDashboardButtonRevealController : public ui::EventHandler {
 public:
  explicit GameDashboardButtonRevealController(GameDashboardContext* context);
  GameDashboardButtonRevealController(
      const GameDashboardButtonRevealController&) = delete;
  GameDashboardButtonRevealController& operator=(
      const GameDashboardButtonRevealController&) = delete;
  ~GameDashboardButtonRevealController() override;

  void StopTopEdgeTimer() { top_edge_hover_timer_.Stop(); }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  friend class GameDashboardContextTestApi;

  // Checks if the Game Dashboard button can be shown. Returns true if its
  // widget is not visible and `mouse_screen_location` is within the Game
  // Dashboard button reveal bounds.
  bool CanShowGameDashboardButton(const gfx::Point& mouse_screen_location);

  // Checks if the Game Dashboard button can be hidden. Returns true if its
  // widget is visible, the Game Dashboard main menu is closed, and
  // `mouse_screen_location` is within the Game Dashboard reveal bounds.
  bool CanHideGameDashboardButton(const gfx::Point& mouse_screen_location);

  // Returns true if `mouse_screen_location` is within the Game Dashboard
  // button's reveal bounds, which is the top section of the window with a
  // height of
  // `chromeos::ImmersiveFullscreenController::kMouseRevealBoundsHeight`.
  bool IsMouseWithinButtonRevealBounds(const gfx::Point& mouse_screen_location);

  // Returns true if `mouse_screen_location` is outside the window's frame
  // header.
  bool IsMouseOutsideHeaderBounds(const gfx::Point& mouse_screen_location);

  // Called when the `top_edge_hover_timer_` is fired.
  void OnTopEdgeHoverTimeout();

  const raw_ptr<GameDashboardContext> context_;

  // Timer to track cursor being held at the top edge of the screen.
  base::OneShotTimer top_edge_hover_timer_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_
