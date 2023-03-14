// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window_observer.h"

namespace ash {

class GameDashboardSession;

// The game dashboard controller is responsible for creating and managing game
// dashboard sessions. It provides a means to start a session on the focused
// window, if that window is a relevant game surface. The session can also be
// stopped.
class ASH_EXPORT GameDashboardController : public SessionObserver,
                                           public aura::WindowObserver {
 public:
  GameDashboardController();
  GameDashboardController(const GameDashboardController&) = delete;
  GameDashboardController& operator=(const GameDashboardController&) = delete;
  ~GameDashboardController() override;

  // Returns true if this window supports starting the game dashboard. Otherwise
  // returns false.
  static bool CanStart(aura::Window* window);

  // Returns true if there is an active game dashboard session associated
  // with the given window. Otherwise returns false.
  bool IsActive(aura::Window* window) const;

  // If there is not an active game dashboard session for the given window,
  // starts one and returns true. If there is already an active session, this
  // method returns false.
  bool Start(aura::Window* window);

  // If there is an active game dashboard session for the given window, stops
  // that session. Otherwise does nothing.
  void Stop(aura::Window* window);

  // If there is an active game dashboard session for the given window, toggles
  // the associated game dashboard menu. Otherwise does nothing.
  void ToggleMenu(aura::Window* window);

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnChromeTerminating() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Calls Shutdown on all active sessions and clears the session map. Generally
  // this is done on logged in user change or Chrome terminating.
  void ShutdownAllSessions();

  base::flat_map<aura::Window*, std::unique_ptr<GameDashboardSession>>
      sessions_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
