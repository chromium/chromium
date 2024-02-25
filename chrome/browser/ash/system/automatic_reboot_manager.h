// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/system/automatic_reboot_manager_observer.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/base/user_activity/user_activity_observer.h"

class PrefRegistrySimple;

namespace base {
class TickClock;
}

namespace ash {
namespace system {

namespace internal {
struct SystemEventTimes;
}

// Schedules and executes automatic reboots.
//
// Automatic reboots may be scheduled for any number of reasons. Currently, the
// following are implemented:
// * When Chrome OS has applied a system update, a reboot may become necessary
//   to complete the update process. If the policy to automatically reboot after
//   an update is enabled, a reboot is scheduled at that point.
// * If an uptime limit is set through policy, a reboot is scheduled when the
//   device's uptime reaches the limit. Time spent sleeping counts as uptime as
//   well.
//
// When the time of the earliest scheduled reboot is reached, the reboot is
// requested. The reboot is performed immediately unless one of the following
// reasons inhibits it:
// * If the login screen is being shown: Reboots are inhibited while the user is
//   interacting with the screen (determined by checking whether there has been
//   any user activity in the past 60 seconds).
// * If a session is in progress: Reboots are inhibited until the session ends,
//   the browser is restarted or the device is suspended.
//
// If reboots are inhibited, a 24 hour grace period is started. The reboot
// request is carried out the moment none of the inhibiting criteria apply
// anymore (e.g. the user becomes idle on the login screen, the user logs exits
// a session, the user suspends the device). If reboots remain inhibited for the
// entire grace period, a reboot is performed at its end, unless a non-kiosk
// session is active.
//
// Reboots may be scheduled and canceled at any time. This causes the time at
// which a reboot should be requested and the grace period that follows it to
// be recalculated.
//
// Reboots are scheduled in terms of device uptime. The current uptime is read
// from /proc/uptime. The time at which a reboot became necessary to finish
// applying an update is stored in /var/run/chrome/update_reboot_needed_uptime,
// making it persist across browser restarts and crashes. Placing the file under
// /var/run ensures that it gets cleared automatically on every boot.
class AutomaticRebootManager : public chromeos::PowerManagerClient::Observer,
                               public UpdateEngineClient::Observer,
                               public ui::UserActivityObserver,
                               public session_manager::SessionManagerObserver {
 public:
  AutomaticRebootManager(const base::Clock* clock,
                         const base::TickClock* tick_clock);

  AutomaticRebootManager(const AutomaticRebootManager&) = delete;
  AutomaticRebootManager& operator=(const AutomaticRebootManager&) = delete;

  ~AutomaticRebootManager() override;

  AutomaticRebootManagerObserver::Reason reboot_reason() const {
    return reboot_reason_;
  }
  bool reboot_requested() const { return reboot_requested_; }

  void AddObserver(AutomaticRebootManagerObserver* observer);
  void RemoveObserver(AutomaticRebootManagerObserver* observer);

  // Blocks until Init() is called and then returns true. If Init() is not
  // called within |timeout|, returns false.
  bool WaitForInitForTesting(const base::TimeDelta& timeout);

  // PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // UpdateEngineClient::Observer:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class AutomaticRebootManagerBasicTest;

  // Finishes initialization. Called after the |system_event_times| have been
  // loaded in the blocking thread pool.
  void Init(const internal::SystemEventTimes& system_event_times);

  // Reschedules the reboot request, start and end of the grace period. Reboots
  // immediately if the end of the grace period has already passed.
  void Reschedule();

  // Requests a reboot.
  void RequestReboot();

  // Called whenever the status of the criteria inhibiting reboots may have
  // changed. Reboots immediately if a reboot has actually been requested and
  // none of the criteria inhibiting it apply anymore. Otherwise, does nothing.
  // If |ignore_session|, a session in progress does not inhibit reboots.
  void MaybeReboot(bool ignore_session);

  // Reboots immediately unless a non-kiosk session is active.
  void Reboot();

  // Callback invoked when Chrome shuts down.
  void OnAppTerminating();

  // Event that is signaled when Init() runs.
  base::WaitableEvent initialized_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  // Clocks that can be mocked in tests to fast-forward time.
  const raw_ptr<const base::Clock> clock_;
  const raw_ptr<const base::TickClock> tick_clock_;

  PrefChangeRegistrar local_state_registrar_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // Fires when the user has been idle on the login screen for a set amount of
  // time.
  std::unique_ptr<base::OneShotTimer> login_screen_idle_timer_;

  // The time at which the device was booted, in |tick_clock_| ticks.
  std::optional<base::TimeTicks> boot_time_;

  // The time at which an update was applied and a reboot became necessary to
  // complete the update process, in |tick_clock_| ticks.
  std::optional<base::TimeTicks> update_reboot_needed_time_;

  // The reason for the reboot request. Updated whenever a reboot is scheduled.
  AutomaticRebootManagerObserver::Reason reboot_reason_ =
      AutomaticRebootManagerObserver::REBOOT_REASON_UNKNOWN;

  // Whether a reboot has been requested.
  bool reboot_requested_ = false;

  // Timers that start and end the grace period.
  std::unique_ptr<base::WallClockTimer> grace_start_timer_;
  std::unique_ptr<base::WallClockTimer> grace_end_timer_;

  base::ObserverList<AutomaticRebootManagerObserver, true>::Unchecked
      observers_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::WeakPtrFactory<AutomaticRebootManager> weak_ptr_factory_{this};
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_
