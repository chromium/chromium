// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

namespace policy {

// This class schedules timers for showing notification and dialog when
// scheduled reboot policy is set.
class RebootNotificationsScheduler {
 public:
  RebootNotificationsScheduler();
  RebootNotificationsScheduler(const RebootNotificationsScheduler&) = delete;
  RebootNotificationsScheduler& operator=(const RebootNotificationsScheduler&) =
      delete;
  virtual ~RebootNotificationsScheduler();

  // Schedules timers for showing notification and dialog or shows them right
  // away if the scheduled reboot time is soon. Notifications are not shown when
  // grace time applies.
  void ScheduleNotifications(base::OnceClosure reboot_callback,
                             const base::Time& reboot_time);

  // Resets timers and closes notification and dialog if open.
  void ResetState();

  // Grace time applies if the reboot is scheduled in less then an hour from the
  // last device reboot.
  bool ShouldApplyGraceTime(const base::Time& reboot_time) const;

 protected:
  RebootNotificationsScheduler(const base::Clock* clock,
                               const base::TickClock* tick_clock);

  // Runs |reboot_callback_| when user clicks on "Reboot now" button of the
  // dialog or notification.
  void OnRebootButtonClicked();

 private:
  virtual void MaybeShowNotification();
  virtual void MaybeShowDialog();

  // Returns current time.
  virtual const base::Time GetCurrentTime() const;

  // Returns time since last reboot.
  virtual const base::TimeDelta GetSystemUptime() const;

  // Returns delay from now until |reboot_time|.
  base::TimeDelta GetRebootDelay(const base::Time& reboot_time) const;

  // Timers for scheduling notification or dialog displaying.
  base::WallClockTimer notification_timer_, dialog_timer_;
  // Controller responsible for creating notifications and dialog.
  RebootNotificationController notification_controller_;
  // Scheduled reboot time.
  base::Time reboot_time_;
  // Callback to run on "Reboot now" button click.
  base::OnceClosure reboot_callback_;
  base::WeakPtrFactory<RebootNotificationsScheduler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_REBOOT_NOTIFICATIONS_SCHEDULER_H_