// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

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

  // Updates the visibility of the Game Dashboard button widget. If
  // `target_visibility` is true, then the widget will be visible, otherwise it
  // will not be visible. If `animate` is true, the widget will slide up/down
  // with a short animation to `target_visibility`, otherwise, the widget will
  // move without any animation.
  void UpdateVisibility(bool target_visibility, bool animate);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

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

  // Returns true if `event_screen_location` is within the Game Dashboard
  // button's reveal bounds, which is the top section of the window with the
  // given `reveal height`.
  bool IsEventWithinButtonRevealBounds(const gfx::Point& event_screen_location,
                                       int reveal_height);

  // Returns true if `mouse_screen_location` is outside the window's frame
  // header.
  bool IsMouseOutsideHeaderBounds(const gfx::Point& mouse_screen_location);

  // Called when the `top_edge_hover_timer_` is fired.
  void OnTopEdgeHoverTimeout();

  // Callbacks when the animation has ended in `UpdateVisibility()`.
  void OnAnimationEnd(bool target_visibility);

  const raw_ptr<GameDashboardContext> context_;

  // Timer to track cursor being held at the top edge of the screen.
  base::OneShotTimer top_edge_hover_timer_;

  // Gesture scroll begin position when drag starts.
  std::optional<gfx::Point> gesture_scroll_start_pos_;

  base::WeakPtrFactory<GameDashboardButtonRevealController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_REVEAL_CONTROLLER_H_
