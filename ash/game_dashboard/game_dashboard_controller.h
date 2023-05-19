// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/game_dashboard/game_dashboard_delegate.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Controls the Game Dashboard behavior on supported windows.
class ASH_EXPORT GameDashboardController : public aura::EnvObserver,
                                           public aura::WindowObserver,
                                           public CaptureModeObserver {
 public:
  explicit GameDashboardController(
      std::unique_ptr<GameDashboardDelegate> delegate);
  GameDashboardController(const GameDashboardController&) = delete;
  GameDashboardController& operator=(const GameDashboardController&) = delete;
  ~GameDashboardController() override;

  // Returns the singleton instance owned by `Shell`.
  static GameDashboardController* Get();

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // CaptureModeObserver:
  void OnRecordingStarted(aura::Window* current_root) override;
  void OnRecordingEnded() override;
  void OnVideoFileFinalized(bool user_deleted_video_file,
                            const gfx::ImageSkia& thumbnail) override;
  void OnRecordedWindowChangingRoot(aura::Window* new_root) override;
  void OnRecordingStartAborted() override;

 private:
  enum class WindowGameState { kGame, kNotGame, kNotYetKnown };

  friend class GameDashboardControllerTest;

  // Checks to see if the given window is a game. If there's not enough
  // information, then returns `kNotYetKnown`, otherwise returns `kGame` or
  // `kNotGame`.
  WindowGameState GetWindowGameState(aura::Window* window) const;

  // Updates the window observation, depending on whether the given window is a
  // game or not.
  void RefreshWindowTracking(aura::Window* window);

  // The delegate responsible for communicating with between Ash and the Game
  // Dashboard service in the browser.
  std::unique_ptr<GameDashboardDelegate> delegate_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
