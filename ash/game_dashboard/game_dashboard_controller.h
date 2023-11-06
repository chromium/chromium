// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_delegate.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
class WindowTracker;
}  // namespace aura

namespace ash {

// Controls the Game Dashboard behavior on supported windows.
class ASH_EXPORT GameDashboardController : public aura::EnvObserver,
                                           public aura::WindowObserver,
                                           public CaptureModeObserver,
                                           public OverviewObserver {
 public:
  explicit GameDashboardController(
      std::unique_ptr<GameDashboardDelegate> delegate);
  GameDashboardController(const GameDashboardController&) = delete;
  GameDashboardController& operator=(const GameDashboardController&) = delete;
  ~GameDashboardController() override;

  // Returns the singleton instance owned by `Shell`.
  static GameDashboardController* Get();

  // Checks whether the `window` is a game.
  static bool IsGameWindow(aura::Window* window);

  // Checks whether the `window` can respond to accelerator commands.
  static bool ReadyForAccelerator(aura::Window* window);

  // Gets the app name by `app_id`.
  std::string GetArcAppName(const std::string& app_id) const;

  GameDashboardContext* active_recording_context() {
    return active_recording_context_;
  }

  // Returns a pointer to the `GameDashboardContext` if the given `window` is a
  // game window, otherwise nullptr.
  GameDashboardContext* GetGameDashboardContext(aura::Window* window) const;

  // Represents the start of the `context`'s game window capture session.
  // Sets `context` as the `active_recording_context_`, and requests
  // `CaptureModeController` to start a capture session for the `context`'s game
  // window. If `record_instantly` is true, instantly starts the capture
  // session, skipping the countdown timer UI. The session ends when
  // `OnRecordingEnded` or `OnRecordingStartAborted` is called.
  void StartCaptureSession(GameDashboardContext* context,
                           bool record_instantly = false);

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // CaptureModeObserver:
  void OnRecordingStarted(aura::Window* current_root) override;
  void OnRecordingEnded() override;
  void OnVideoFileFinalized(bool user_deleted_video_file,
                            const gfx::ImageSkia& thumbnail) override;
  void OnRecordedWindowChangingRoot(aura::Window* new_root) override;
  void OnRecordingStartAborted() override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeEnded() override;

 private:
  friend class GameDashboardControllerTest;
  friend class GameDashboardTestBase;

  enum class WindowGameState { kGame, kNotGame, kNotYetKnown };

  using GetWindowStateCallback = base::OnceCallback<void(WindowGameState)>;

  // Checks whether the given window is a game, and then calls
  // `RefreshWindowTracking`. If there's not enough information, it passes
  // `kNotYetKnown`, otherwise `kGame` or `kNotGame`, as the `game_state`.
  void GetWindowGameState(aura::Window* window);

  // Callback when `GetWindowGameState` calls `GameDashboardDelegate` to
  // retrieve the app category for the given window in `window_tracker`.
  // This function calls `RefreshWindowTracking`, as long as the window has not
  // been destroyed.
  void OnArcWindowIsGame(std::unique_ptr<aura::WindowTracker> window_tracker,
                         bool is_game);

  // Updates the window observation, dependent on `game_state`.
  void RefreshWindowTracking(aura::Window* window, WindowGameState game_state);

  // Updates the Game Dashboard button state and toolbar for a game window.
  void RefreshForGameControlsFlags(aura::Window* window);

  std::map<aura::Window*, std::unique_ptr<GameDashboardContext>>
      game_window_contexts_;

  // The delegate responsible for communicating with between Ash and the Game
  // Dashboard service in the browser.
  std::unique_ptr<GameDashboardDelegate> delegate_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  // Represents the active `GameDashboardContext`. If
  // `active_recording_context_` is non-null, then `CaptureModeController` is
  // recording the game window, or has been requested to record it. Resets
  // when the recording session ends or aborted.
  // Owned by `game_window_contexts_`.
  raw_ptr<GameDashboardContext, DanglingUntriaged | ExperimentalAsh>
      active_recording_context_ = nullptr;

  base::WeakPtrFactory<GameDashboardController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
