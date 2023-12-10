// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace policy {

// This class schedules timers for showing pending reboot notification and
// dialog when scheduled reboot policy is set. The class also schedules
// post-reboot notification shown to the user after the policy reboot. If the
// full restore service is available for the user profile, post reboot
// notification is integrated with full restore notification. Otherwise, a
// simple post reboot notification is shown.
class RebootNotificationsScheduler
    : public session_manager::SessionManagerObserver {
 public:
  // Represents the source of notification request. `kMaxValue` is used to
  // determine the number of items, update it when adding a new item.
  enum class Requester {
    kScheduledRebootPolicy,
    kRebootCommand,
    kMaxValue = kRebootCommand
  };

  using RebootButtonCallback = base::OnceClosure;

  RebootNotificationsScheduler();
  RebootNotificationsScheduler(const RebootNotificationsScheduler&) = delete;
  RebootNotificationsScheduler& operator=(const RebootNotificationsScheduler&) =
      delete;
  ~RebootNotificationsScheduler() override;

  // Returns current RebootNotificationsScheduler instance or NULL if it hasn't
  // been initialized yet.
  static RebootNotificationsScheduler* Get();

  // Registers boolean pref for showing post reboot notification.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if the pref for showing the post reboot notification is set
  // for the |profile|.
  static bool ShouldShowPostRebootNotification(Profile* profile);

  // Schedules timers for showing pending reboot notification and dialog or
  // shows them right away if the scheduled reboot time is soon. If there
  // already is a scheduled notification, either reschedules notification or
  // puts the new one in the pending queue. Always picks the earliest
  // notification from the queue and the new one.
  void SchedulePendingRebootNotifications(
      RebootButtonCallback reboot_button_callback,
      const base::Time& reboot_time,
      Requester requester);

  // Sets pref for showing the post reboot notification for the active user.
  void SchedulePostRebootNotification();

  // Resets the state of `requester`. If it is the current requester showing
  // notifications, takes another requester from the pending queue. Otherwise,
  // removes `requester` from the pending queue if present.
  void CancelRebootNotifications(Requester requester);

  // SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // Shows simple post reboot notification if |show_simple_notification| flag is
  // set to true and unsets the pref for showing the post reboot notification.
  void MaybeShowPostRebootNotification(bool show_simple_notification);

 protected:
  RebootNotificationsScheduler(const base::Clock* clock,
                               const base::TickClock* tick_clock);

  // Runs |reboot_callback_| when user clicks on "Reboot now" button of the
  // dialog or notification.
  void OnRebootButtonClicked();

  std::optional<Requester> GetCurrentRequesterForTesting() const;

  std::vector<Requester> GetRequestersForTesting() const;

  // Sets RebootNotificationsScheduler instance.
  static void SetInstance(
      RebootNotificationsScheduler* reboot_notifications_scheduler);

 private:
  // Queue of notification requests prioritized by reboot time from the earliest
  // to the latest.
  class RequestQueue;

  void SchedulePendingRebootNotificationsForCurrentRequester();
  virtual void MaybeShowPendingRebootNotification();
  virtual void MaybeShowPendingRebootDialog();

  // Returns prefs for active profile or nullptr.
  virtual PrefService* GetPrefsForActiveProfile() const;

  // Returns delay from now until |reboot_time|.
  base::TimeDelta GetRebootDelay(const base::Time& reboot_time) const;

  // Closes the pending reboot notification and the reboot dialog.
  virtual void CloseNotifications();

  // Returns true if the full restore service is available for the profile and
  // we need to wait for full restore service initialization.
  virtual bool ShouldWaitFullRestoreInit() const;

  // Resets timers and closes notification and dialog if open.
  void ResetNotificationState();

  // Returns true if the pref for showing the post reboot notification is set in
  // |prefs|.
  static bool IsPostRebootPrefSet(PrefService* prefs);

  // Pointer to the existing RebootNotificationsScheduler instance (if any). Not
  // owned.
  static RebootNotificationsScheduler* instance;

  std::unique_ptr<RequestQueue> requester_queue_;

  // Timers for scheduling notification or dialog displaying.
  base::WallClockTimer notification_timer_, dialog_timer_;
  // Controller responsible for creating notifications and dialog.
  RebootNotificationController notification_controller_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      observation_{this};

  raw_ptr<const base::Clock> clock_;

  base::WeakPtrFactory<RebootNotificationsScheduler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_
