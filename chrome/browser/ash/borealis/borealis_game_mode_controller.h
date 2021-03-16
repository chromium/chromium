// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_MODE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_MODE_CONTROLLER_H_

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/scoped_observation.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"

namespace borealis {

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
class BorealisGameModeController : public aura::client::FocusChangeObserver {
 public:
  BorealisGameModeController();
  BorealisGameModeController(const BorealisGameModeController&) = delete;
  BorealisGameModeController& operator=(const BorealisGameModeController&) =
      delete;
  ~BorealisGameModeController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // TODO(b/179961266) replace with sending actual messages to enter game mode.
  class ScopedGameMode {
   public:
    ScopedGameMode();
    ~ScopedGameMode();
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

    BorealisGameModeController::ScopedGameMode* GetGameMode();
    void UpdateGameModeStatus(ash::WindowState* window_state);

   private:
    base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
        window_state_observer_{this};
    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observer_{this};
    std::unique_ptr<BorealisGameModeController::ScopedGameMode> game_mode_;
  };

  ScopedGameMode* GetGameModeForTesting();

 private:
  base::ScopedObservation<aura::client::FocusClient,
                          aura::client::FocusChangeObserver>
      root_focus_observer_;
  std::unique_ptr<WindowTracker> focused_ = nullptr;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_MODE_CONTROLLER_H_
