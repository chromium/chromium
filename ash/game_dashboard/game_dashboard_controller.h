// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/game_dashboard/game_dashboard_delegate.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Controls the Game Dashboard behavior on supported windows.
class ASH_EXPORT GameDashboardController {
 public:
  explicit GameDashboardController(
      std::unique_ptr<GameDashboardDelegate> delegate);
  GameDashboardController(const GameDashboardController&) = delete;
  GameDashboardController& operator=(const GameDashboardController&) = delete;
  ~GameDashboardController();

  // Returns the singleton instance owned by `Shell`.
  static GameDashboardController* Get();

  // Returns true if the given window supports the game dashboard.
  bool IsSupported(aura::Window* window) const;

 private:
  // The delegate responsible for communicating with between Ash and the Game
  // Dashboard service in the browser.
  std::unique_ptr<GameDashboardDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
