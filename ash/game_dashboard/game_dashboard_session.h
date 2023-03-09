// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_SESSION_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_SESSION_H_

namespace aura {
class Window;
}

namespace ash {

// A game dashboard session is created for an app that raised the
// game dashboard UI. It should be initialized with the top level
// window of the app in which the dashboard was raised. The session
// may continue even when all visual elements are closed if there is
// active screen recording in progress.
class GameDashboardSession {
 public:
  explicit GameDashboardSession(aura::Window* window);
  GameDashboardSession(const GameDashboardSession&) = delete;
  GameDashboardSession& operator=(const GameDashboardSession&) = delete;
  ~GameDashboardSession();

  bool is_shutting_down() const { return is_shutting_down_; }
  aura::Window* window() const { return window_; }

  // Creates all UI elements that might be needed for this session.
  void Initialize();

  // `Shutdown()` should be called just before the session destructor is called.
  void Shutdown();

  // Toggles the main menu.
  void ToggleMenu();

 private:
  // The top window associated with this session.
  aura::Window* const window_;

  // Once the `Shutdown()` method has been called on this session, this value
  // will be true.
  bool is_shutting_down_ = false;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_SESSION_H_
