// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_START_TIME_TRACKER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_START_TIME_TRACKER_H_

// TODO(crbug.com/473653626): Move to components/session_manager after
// resolving the dependencies.

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/user_activity/user_activity_observer.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class Clock;
}  // namespace base

namespace session_manager {

// Tracks the session start time timestamp. This is expected to be created
// when the user session is newly started.
class SessionStartTimeTracker : public ui::UserActivityObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the session start time is updated.
    virtual void OnSessionStartTimeUpdated() {}

   protected:
    ~Observer() override = default;
  };

  // Neither local_state nor clock must be nullptr. Also, they must outlive
  // this instance.
  SessionStartTimeTracker(PrefService* local_state,
                          const base::Clock* clock,
                          bool browser_restarted);
  SessionStartTimeTracker(const SessionStartTimeTracker&) = delete;
  SessionStartTimeTracker& operator=(const SessionStartTimeTracker&) = delete;
  ~SessionStartTimeTracker() override;

  // Registers local_state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the start time. Maybe null if its not considered as started.
  base::Time GetStartTime() const;

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

  const raw_ref<PrefService> local_state_;
  const raw_ref<const base::Clock> clock_;
  base::ObserverList<Observer> observer_list_;
  PrefChangeRegistrar pref_change_registrar_;
  bool user_activity_seen_ = false;
  base::Time start_time_;
};

}  // namespace session_manager

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_START_TIME_TRACKER_H_
