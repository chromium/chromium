// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_CURSOR_HANDLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_CURSOR_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace wm {
class CursorManager;
}  // namespace wm

namespace ash {

class GameDashboardContext;

// GameDashboardMainMenuCursorHandler is a pretarget handler that when
// registered, forcibly shows the mouse cursor. This class is instantiated and
// registered by `GameDashboardContext`, when the user opens the Game Dashboard
// main menu, and destroyed when it's closed.
class GameDashboardMainMenuCursorHandler : public ui::EventHandler {
 public:
  explicit GameDashboardMainMenuCursorHandler(GameDashboardContext* context);
  GameDashboardMainMenuCursorHandler(
      const GameDashboardMainMenuCursorHandler&) = delete;
  GameDashboardMainMenuCursorHandler& operator=(
      const GameDashboardMainMenuCursorHandler&) = delete;
  ~GameDashboardMainMenuCursorHandler() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  // Returns true if `event` is inside the window's
  // `FrameCaptionButtonContainerView`.
  bool IsEventInWindowFrameHeader(const ui::LocatedEvent& event);

  bool cursor_visibility_to_restore_ = false;
  gfx::NativeCursor cursor_to_restore_;

  const raw_ptr<GameDashboardContext> context_;
  const raw_ptr<wm::CursorManager> cursor_manager_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_MAIN_MENU_CURSOR_HANDLER_H_
