// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"

namespace game_mode {

// When a borealis window enters full screen, game mode is enabled.
// The controller works as follows:
//
//          +"GameMode off"+              "GameMode off"
//          |              |                  |     ^ Not fullscreen
//          |              | N                |     |
//          V   focused    |              Y   V     |   Fullscreen
// "Watch focus"------->"Borealis window?"-->"Watch state"----->"GameMode on"
//         ^                    ^             |   |     ^          |
//         |                    +-------------+   |     +----------+
//         |                    focus changed     |
//         +------"GameMode off"<-----------------+
//                                No window focused
//
class GameModeController : public aura::client::FocusChangeObserver {
 public:
  GameModeController();
  GameModeController(const GameModeController&) = delete;
  GameModeController& operator=(const GameModeController&) =
      delete;
  ~GameModeController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  class GameModeEnabler {
   public:
    GameModeEnabler();
    ~GameModeEnabler();

   private:
    static void OnSetGameMode(
        bool was_refresh,
        absl::optional<ash::ResourcedClient::GameMode> previous);
    void RefreshGameMode();

    // Used to determine if it's the first instance of game mode failing.
    static bool should_record_failure;
    base::RepeatingTimer timer_;
  };

  class WindowTracker : public ash::WindowStateObserver,
                        public aura::WindowObserver {
   public:
    WindowTracker(ash::WindowState* window_state,
                  std::unique_ptr<WindowTracker> previous_focus);
    ~WindowTracker() override;

    // Overridden from WindowObserver
    void OnWindowDestroying(aura::Window* window) override;

    // Overridden from WindowStateObserver
    void OnPostWindowStateTypeChange(
        ash::WindowState* window_state,
        chromeos::WindowStateType old_type) override;

    void UpdateGameModeStatus(ash::WindowState* window_state);

   private:
    base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
        window_state_observer_{this};
    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observer_{this};
    std::unique_ptr<GameModeController::GameModeEnabler> game_mode_;
  };

 private:
  std::unique_ptr<WindowTracker> focused_;
};

}  // namespace game_mode

#endif  // CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_
