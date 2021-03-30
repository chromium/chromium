// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SESSION_LENGTH_LIMITER_H_
#define CHROME_BROWSER_CHROMEOS_SESSION_LENGTH_LIMITER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/util/timer/wall_clock_timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/user_activity/user_activity_observer.h"

class PrefRegistrySimple;

namespace chromeos {

// Enforces a session length limit by terminating the session when the limit is
// reached.
class SessionLengthLimiter : public ui::UserActivityObserver {
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
  ~SessionLengthLimiter() override;

  // Returns the duration between |session_start_time_| and now if there is a
  // valid |session_start_time_|. Otherwise, returns 0.
  base::TimeDelta GetSessionDuration() const;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

 private:
  // Attempt to restore the session start time and the flag indicating user
  // activity from local state. Return |true| if the restore is successful.
  bool RestoreStateAfterCrash();

  // Update the session start time if possible:
  // * If instructed to wait for initial user activity, the session start time
  //   advances every time this method is called as long as no user activity has
  //   occurred yet. The time is not persisted in local state.
  // * If instructed not to wait for initial user activity, the session start
  //   time is set and persisted in local state the first time this method is
  //   called.
  // The pref indicating whether to wait for initial user activity may change at
  // any time, switching between the two behaviors.
  void UpdateSessionStartTime();

  void UpdateLimit();

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Delegate> delegate_;
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<util::WallClockTimer> timer_;
  base::Time session_start_time_;
  bool user_activity_seen_;

  DISALLOW_COPY_AND_ASSIGN(SessionLengthLimiter);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove once the migratin is finished.
namespace ash {
using ::chromeos::SessionLengthLimiter;
}

#endif  // CHROME_BROWSER_CHROMEOS_SESSION_LENGTH_LIMITER_H_
