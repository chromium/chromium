// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_LENGTH_LIMITER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_LENGTH_LIMITER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/session/session_start_time_tracker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"

class PrefRegistrySimple;

namespace base {
class Clock;
}  // namespace base

namespace ash {

// Enforces a session length limit by terminating the session when the limit is
// reached.
class SessionLengthLimiter
    : public session_manager::SessionStartTimeTracker::Observer {
 public:
  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // `local_state`, `clock` and `session_manager` must not be nullptr, and they
  // must outlive this instance.
  SessionLengthLimiter(PrefService* local_state,
                       base::Clock* clock,
                       session_manager::SessionManager* session_manager,
                       bool browser_restarted);

  SessionLengthLimiter(const SessionLengthLimiter&) = delete;
  SessionLengthLimiter& operator=(const SessionLengthLimiter&) = delete;

  ~SessionLengthLimiter() override;

  // Returns the duration between |session_start_time_| and now if there is a
  // valid |session_start_time_|. Otherwise, returns 0.
  base::TimeDelta GetSessionDuration() const;

  // SessionStartTimeTracker::Observer
  void OnSessionStartTimeUpdated() override;

  // Injects the base::Clock used in this instance for testing purpose.
  // On destruction of returned AutoReset, it will be reset.
  // `clock` must not be nullptr, and must be alive until resetting.
  base::AutoReset<raw_ref<base::Clock>> SetClockForTesting(base::Clock* clock);

 private:
  void UpdateLimit();

  base::ThreadChecker thread_checker_;

  raw_ref<base::Clock> clock_;
  const raw_ref<session_manager::SessionManager> session_manager_;
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
