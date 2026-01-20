// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_LENGTH_LIMITER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_LENGTH_LIMITER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/session/session_start_time_tracker.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace ash {

// Enforces a session length limit by terminating the session when the limit is
// reached.
class SessionLengthLimiter
    : public session_manager::SessionStartTimeTracker::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual const base::Clock* GetClock() const = 0;
    virtual void StopSession() = 0;
  };

  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  SessionLengthLimiter(Delegate* delegate, bool browser_restarted);

  SessionLengthLimiter(const SessionLengthLimiter&) = delete;
  SessionLengthLimiter& operator=(const SessionLengthLimiter&) = delete;

  ~SessionLengthLimiter() override;

  // Returns the duration between |session_start_time_| and now if there is a
  // valid |session_start_time_|. Otherwise, returns 0.
  base::TimeDelta GetSessionDuration() const;

  // SessionStartTimeTracker::Observer
  void OnSessionStartTimeUpdated() override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  void UpdateLimit();

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<session_manager::SessionStartTimeTracker>
      session_start_time_tracker_;
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<base::WallClockTimer> timer_;

  base::ScopedObservation<session_manager::SessionStartTimeTracker,
                          session_manager::SessionStartTimeTracker::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_LENGTH_LIMITER_H_
