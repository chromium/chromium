// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_SESSION_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_SESSION_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/time/time.h"

namespace ash {

// Represents a Focus Mode session and provides convenience functions to
// determine the current state and increase the duration.
class ASH_EXPORT FocusModeSession {
 public:
  enum class State {
    kOff,     // There is no active session.
    kOn,      // The session is currently running.
    kEnding,  // The session is in the ending state and will soon finish.
  };

  // A snapshot of the current focus mode session.
  struct Snapshot {
    State state;
    base::TimeDelta session_duration;
    base::TimeDelta time_elapsed;
    base::TimeDelta remaining_time;
    double progress;
  };

  // Create a session that lasts for `session_duration` and ends at `end_time`.
  FocusModeSession(const base::TimeDelta& session_duration,
                   const base::Time& end_time);
  FocusModeSession(const FocusModeSession&) = default;
  FocusModeSession& operator=(const FocusModeSession&) = default;
  ~FocusModeSession() = default;

  base::Time end_time() const { return end_time_; }
  // TODO(b/318897434): This is only needed for the accelerator to trigger an
  // ending moment immediately. Remove this after testing is complete.
  void set_end_time(base::Time end_time) { end_time_ = end_time; }
  base::TimeDelta session_duration() const { return session_duration_; }
  bool persistent_ending() const { return persistent_ending_; }
  // This will cause `GetState` to always return `kEnding` even if the session
  // is past the ending moment duration. `ExtendSession` unlocks this
  // implicitly.
  void set_persistent_ending() { persistent_ending_ = true; }

  // Returns the corresponding state for the given timestamp.
  State GetState(const base::Time& now) const;

  // Creates a snapshot of the focus session.
  Snapshot GetSnapshot(const base::Time& now) const;

  // Extends the session duration by `focus_mode_util::kExtendDuration`, up to
  // `focus_mode_util::kMaximumDuration`. The behavior of this function depends
  // on state, which depends on `now`.
  void ExtendSession(const base::Time& now);

  // Returns the difference between `now` and the end time of the session. If
  // the delta would be negative, 0 is returned.
  base::TimeDelta GetTimeRemaining(const base::Time& now) const;

 private:
  // This is the expected duration of a Focus Mode session once it starts.
  // Depends on previous session data (from user prefs) or user input.
  base::TimeDelta session_duration_;

  // The end time of an active Focus Mode session.
  base::Time end_time_;

  // This is used to determine if the session will be in the ending moment
  // indefinitely. Used to allow the tray bubble to be shown without timing out,
  // and to potentially prevent the `FocusModeController` from closing the
  // contextual panel when the ending moment timer expires.
  bool persistent_ending_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_SESSION_H_
