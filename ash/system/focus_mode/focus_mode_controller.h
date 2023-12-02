// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
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

    // Notifies clients every time `SetSessionDuration` is called.
    virtual void OnSessionDurationChanged() {}
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
  base::TimeDelta previous_session_end_duration() const {
    return previous_session_end_duration_;
  }
  base::Time end_time() const { return end_time_; }
  bool turn_on_do_not_disturb() const { return turn_on_do_not_disturb_; }
  void set_turn_on_do_not_disturb(bool turn_on) {
    turn_on_do_not_disturb_ = turn_on;
  }
  const std::u16string& selected_task_title() const {
    return selected_task_title_;
  }
  void set_selected_task_title(const std::u16string& selected_task_title) {
    selected_task_title_ = selected_task_title;
  }
  FocusModeTasksProvider& tasks_provider() { return tasks_provider_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ToggleFocusMode();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Extends the focus time by ten minutes by increasing the `end_time_` and
  // `session_duration_`. This is only used during a focus session.
  void ExtendActiveSessionDuration();

  // Extends an expired focus session by ten minutes by clicking the `+10 min`
  // button on the ending moment UI.
  // TODO(b/308695049): Fill in the logic in a follow-up.
  void ExtendExpiredSession() {}

  // Resets the focus session state for when the user manually ends the session,
  // or when the ending moment is terminated.
  // TODO(b/308695049): Fill in the logic in a follow-up.
  void ResetFocusSession() {}

  // Sets a specific value for `session_duration_` and updates `end_time_` only
  // during an active focus session. Also notifies observers that session
  // duration was changed.
  void SetSessionDuration(const base::TimeDelta& new_session_duration);

  // Returns whether the user has ever started a focus session previously.
  bool HasStartedSessionBefore() const;

 private:
  void SetEnabled(bool enabled);

  // Called every time a second passes on `timer_` while the session is active.
  void OnTimerTick();

  // This is called when the active user changes, and is important to update our
  // cached values in case different users have different stored preferences.
  void UpdateFromUserPrefs();

  // Saves the current selected settings to user prefs so we can provide the
  // same set-up the next time the user comes back to Focus Mode.
  void SaveSettingsToUserPrefs();

  // Closes any open system tray bubbles. This is done whenever we start a focus
  // session.
  void CloseSystemTrayBubble();

  // Sets the visibility of the focus tray on the shelf.
  void SetFocusTrayVisibility(bool visible);

  // Gives Focus Mode access to the Google Tasks API.
  FocusModeTasksProvider tasks_provider_;

  // This is the expected duration of a Focus Mode session once it starts.
  // Depends on previous session data (from user prefs) or user input.
  base::TimeDelta session_duration_;

  // The duration that the previous session ended with. Used when we want to
  // extend the recently expired session.
  base::TimeDelta previous_session_end_duration_;

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

  // This is the task title which was created by the user or selected from
  // existing tasks.
  std::u16string selected_task_title_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
