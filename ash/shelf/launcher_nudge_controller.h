// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LAUNCHER_NUDGE_CONTROLLER_H_
#define ASH_SHELF_LAUNCHER_NUDGE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/display/display_observer.h"

namespace base {
class Clock;
class TickClock;
class WallClockTimer;
}  // namespace base

namespace display {
enum class TabletState;
}  // namespace display

class PrefRegistrySimple;
class PrefService;

namespace ash {

class HomeButton;

// Controls the launcher nudge which animates the home button to guide users to
// open the launcher. The animation is implemented in HomeButton class.
class ASH_EXPORT LauncherNudgeController : public SessionObserver,
                                           public AppListControllerObserver,
                                           public display::DisplayObserver {
 public:
  // Maximum number of times that the nudge will be shown to the users.
  static constexpr int kMaxShownCount = 3;

  // To prevent showing the nudge to users right after the home button appears
  // (e.g. at the moment when the user logs in or change to the clamshell mode),
  // a minimum time delta is used here to guarantee the nudge is shown after a
  // certain amount of time since the home button shows up.
  static constexpr base::TimeDelta kMinIntervalAfterHomeButtonAppears =
      base::Minutes(2);

  LauncherNudgeController();
  LauncherNudgeController(const LauncherNudgeController&) = delete;
  LauncherNudgeController& operator=(const LauncherNudgeController&) = delete;
  ~LauncherNudgeController() override;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Gets the home button on the display with id `display_id`.
  static HomeButton* GetHomeButtonForDisplay(int64_t display_id);

  // Gets the number of times that the nudge has been shown.
  static int GetShownCount(PrefService* prefs);

  // Returns the time delta between user's first login and the first time
  // showing the nudge if `is_first_time` is true. Otherwise, returns the time
  // delta between each time showing the nudge to the user. If the
  // `kLauncherNudgeShortInterval` feature is enabled, use a short interval for
  // testing.
  base::TimeDelta GetNudgeInterval(bool is_first_time) const;

  // Sets custom Clock and TickClock for testing. Note that this should be
  // called before `show_nudge_timer_` has ever started.
  void SetClockForTesting(const base::Clock* clock,
                          const base::TickClock* timer_clock);

  // Checks whether the `show_nudge_timer_` is running.
  bool IsRecheckTimerRunningForTesting();

 private:
  // Checks whether the nudge should be shown. Update the `recheck_time` for
  // MaybeShowNudge() to recheck at `recheck_time`.
  bool ShouldShowNudge(base::Time& recheck_time) const;

  // Updates the user preference data after the nudge is shown.
  void HandleNudgeShown();

  // Shows the nudge immediately if needed. Schedules a task to recheck
  // whether the nudge should be shown at a later time.
  void MaybeShowNudge();

  // Starts a timer to schedule the next attempt to show nudge.
  void ScheduleShowNudgeAttempt(base::Time recheck_time);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Returns the current time. The clock may be overridden for testing.
  base::Time GetNow() const;

  raw_ptr<const base::Clock> clock_for_test_ = nullptr;

  // The timer that keeps track of when should the nudge be shown next time.
  std::unique_ptr<base::WallClockTimer> show_nudge_timer_;

  // The earliest available time for the nudge to be shown next time. Could be
  // null or earlier than GetNow().
  base::Time earliest_available_time_;

  ScopedSessionObserver session_observer_{this};
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<LauncherNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_LAUNCHER_NUDGE_CONTROLLER_H_
