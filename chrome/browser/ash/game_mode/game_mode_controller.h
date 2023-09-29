// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"

namespace game_mode {

namespace {

using GameMode = ash::ResourcedClient::GameMode;

typedef base::RepeatingCallback<void(GameMode)> NotifySetGameModeCallback;

}  // namespace

inline const char* TimeInGameModeHistogramName(GameMode mode) {
  if (mode == GameMode::BOREALIS)
    return "GameMode.TimeInGameMode.Borealis";
  DCHECK(mode == GameMode::ARC);
  return "GameMode.TimeInGameMode.Arc";
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GameModeResult {
  kAttempted = 0,
  kFailed = 1,
  kMaxValue = kFailed,
};

inline const char* GameModeResultHistogramName(GameMode mode) {
  if (mode == GameMode::BOREALIS)
    return "GameMode.Result.Borealis";
  DCHECK(mode == GameMode::ARC);
  return "GameMode.Result.Arc";
}

// When a Borealis or ARC game app game enters full screen, game mode is
// enabled. Game Mode is actually enabled as a result of multiple sets of
// criteria being fulfilled, each checked in sequence. The GameModeController
// additionally exposes an Observer interface which allows clients to subscribe
// to changes in the game mode state.
//
// When one criteria set is met, a new criteria object is constructed which
// is responsible for checking the next criteria, and is owned by the prior
// criteria object. An owner destroys its direct and indirect owned (subsequent)
// criteria objects as soon as itself becomes invalid.
//
// The criteria objects are constructed in this order:
//
// Criteria object         Conditions checked      Causes invalidation (x)
// -----------------------------------------------------------------------------
// GameModeController      Window is focused
// WindowTracker *         Window is fullscreen    Window is destroyed
// ArcGameModeCriteria **  ARC app is game
// GameModeEnabler ***     None
//
// (x) Indicates the responsible criteria object makes itself inactive and
//     discards its child criteria, if any.
// *   WindowTracker is responsible for determining the type of window
// **  ArcGameModeCriteria is not constructed for Borealis windows. Instead, a
//     GameModeEnabler is constructed directly.
// *** GameModeEnabler starts game mode on construction, and stops game mode on
//     destruction.
//
// More concretely, this is the logical flow:
//
//          +"GameMode off"+<---------------------------------------------+
//          |              ^ focus                                  focus ^
//          |              | lost                                   lost  |
//          V   focused    |                                   Y          |
// "Watch focus"------->"Watch state"--------->"Game window?"---->"GameMode on"
//                          ^         full           |                    |
//                          |       screen'd         | N       fullscreen |
//                          |                        V               lost V
//                     "GameMode off"<------------------------------------+
//
class GameModeController : public aura::client::FocusChangeObserver {
 public:
  GameModeController();
  GameModeController(const GameModeController&) = delete;
  GameModeController& operator=(const GameModeController&) = delete;
  ~GameModeController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Represents game mode trigger criteria which, when destroyed, cause game
  // mode to be turned off.
  class GameModeCriteria {
   public:
    virtual ~GameModeCriteria() = default;

    virtual GameMode mode() const = 0;
  };

  // Maintains GameMode in an ON state until destroyed. This is a special case
  // of GameModeCriteria which is always true.
  class GameModeEnabler : public GameModeCriteria {
   public:
    // `signal_resourced` indicates resourced will be notified of the game mode
    // state. Metrics on the amount of time spent in game mode are recorded
    // by the GameModeEnabler regardless of resourced signaling, which allows
    // A/B testing of the effect of optimizations on time spent playing the
    // game.
    GameModeEnabler(GameMode mode,
                    bool signal_resourced,
                    NotifySetGameModeCallback notify_set_game_mode_callback);
    ~GameModeEnabler() override;

    GameMode mode() const override;

   private:
    static void OnSetGameMode(absl::optional<GameMode> refresh_of,
                              absl::optional<GameMode> previous);
    void RefreshGameMode();

    // Used to determine if it's the first instance of game mode failing.
    static bool should_record_failure;
    base::RepeatingTimer timer_;
    base::ElapsedTimer began_;

    const GameMode mode_;
    const bool signal_resourced_;
    const NotifySetGameModeCallback notify_set_game_mode_callback_;
  };

  static GameMode ModeOfWindow(aura::Window* window);

  class WindowTracker : public ash::WindowStateObserver,
                        public aura::WindowObserver {
   public:
    WindowTracker(ash::WindowState* window_state,
                  std::unique_ptr<WindowTracker> previous_focused,
                  NotifySetGameModeCallback notify_set_game_mode_callback);
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
    std::unique_ptr<GameModeCriteria> game_mode_criteria_;
    const NotifySetGameModeCallback notify_set_game_mode_callback_;
  };

  // Observer class which subscribes to changes in the GameMode state.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSetGameMode(GameMode game_mode) = 0;
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);
  void NotifySetGameMode(GameMode game_mode);

 private:
  std::unique_ptr<WindowTracker> focused_;
  base::ObserverList<Observer> observers_;
};

}  // namespace game_mode

#endif  // CHROME_BROWSER_ASH_GAME_MODE_GAME_MODE_CONTROLLER_H_
