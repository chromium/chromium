// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;

namespace ash {

// Controls starting and ending a Focus Mode session and its behavior. Also
// keeps track of the system state to restore after a Focus Mode session ends.
// Has a timer that runs while a session is active and notifies `observers_` on
// every timer tick.
class ASH_EXPORT FocusModeController : public SessionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever Focus Mode changes as a result of user action or when the
    // Focus Mode timer expires.
    virtual void OnFocusModeChanged(bool in_focus_session) = 0;

    // Called every `timer_` tick for updating UI elements during a Focus Mode
    // session.
    virtual void OnTimerTick() {}
  };

  FocusModeController();
  FocusModeController(const FocusModeController&) = delete;
  FocusModeController& operator=(const FocusModeController&) = delete;
  ~FocusModeController() override;

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static FocusModeController* Get();

  // Registers user profile prefs with the specified `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  bool in_focus_session() const { return in_focus_session_; }
  base::TimeDelta session_duration() const { return session_duration_; }
  base::Time end_time() const { return end_time_; }
  bool turn_on_do_not_disturb() const { return turn_on_do_not_disturb_; }
  void set_turn_on_do_not_disturb(bool turn_on) {
    turn_on_do_not_disturb_ = turn_on;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ToggleFocusMode();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  void OnTimerTick();

  // This is called when the active user changes, and is important to update our
  // cached values in case different users have different stored preferences.
  void UpdateFromUserPrefs();

  // Saves the current selected settings to user prefs so we can provide the
  // same set-up the next time the user comes back to Focus Mode.
  void SaveSettingsToUserPrefs();

  // Sets the visibility of the focus tray on the shelf.
  void SetFocusTrayVisibility(bool visible);

  // This is the expected duration of a Focus Mode session once it starts.
  // Depends on previous session data (from user prefs) or user input.
  base::TimeDelta session_duration_;

  // The end time of an active Focus Mode session. `end_time_` is set when we
  // start a session.
  base::Time end_time_;

  // This timer is used for keeping track of the Focus Mode session duration and
  // will trigger a callback every second during a session. It will terminate
  // once the session exceeds `end_time_` or if a user toggles off Focus Mode.
  base::MetronomeTimer timer_;

  // True if the user is currently in an active Focus Mode session.
  bool in_focus_session_ = false;

  // This will dictate whether DND will be turned on when a Focus Mode session
  // starts. Depends on previous session data (from user prefs) or user input.
  bool turn_on_do_not_disturb_ = true;

  // When a Focus Mode session starts, the previous DND state is stored so that
  // it can be restored when the session ends.
  bool previous_do_not_disturb_state_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
