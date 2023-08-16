// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"

namespace ash {

// Controls starting and ending a Focus Mode session and its behavior. Also
// keeps track of the system state to restore after a Focus Mode session ends.
// Has a timer that runs while a session is active and notifies `observers_` on
// every timer tick.
class ASH_EXPORT FocusModeController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever Focus Mode changes as a result of user action or when the
    // Focus Mode timer expires.
    virtual void OnFocusModeChanged(bool in_focus_session) = 0;

    // Called every `timer_` tick for updating UI elements during a focus
    // session.
    virtual void OnTimerTick() {}
  };

  FocusModeController();
  FocusModeController(const FocusModeController&) = delete;
  FocusModeController& operator=(const FocusModeController&) = delete;
  ~FocusModeController();

  bool in_focus_session() const { return in_focus_session_; }
  base::Time end_time() const { return end_time_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ToggleFocusMode();

 private:
  void OnTimerTick();

  bool in_focus_session_ = false;
  bool previous_do_not_disturb_state_ = false;

  base::Time end_time_;
  // This timer is used for keeping track of the focus session duration and will
  // trigger a callback every second during a focus session. It will terminate
  // once the session exceeds `end_time_` or if a user toggles off focus mode.
  base::MetronomeTimer timer_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
