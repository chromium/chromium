// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_REBOOT_NOTIFICATIONS_SCHEDULER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_REBOOT_NOTIFICATIONS_SCHEDULER_H_

#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace policy {

class FakeRebootNotificationsScheduler : public RebootNotificationsScheduler {
 public:
  FakeRebootNotificationsScheduler(const base::Clock* clock,
                                   const base::TickClock* tick_clock,
                                   PrefService* prefs);
  FakeRebootNotificationsScheduler(const FakeRebootNotificationsScheduler&) =
      delete;
  FakeRebootNotificationsScheduler& operator=(
      const FakeRebootNotificationsScheduler&) = delete;
  ~FakeRebootNotificationsScheduler() override;

  int GetShowDialogCalls() const;
  int GetShowNotificationCalls() const;
  int GetCloseNotificationCalls() const;
  void SetUptime(base::TimeDelta uptime);
  void SimulateRebootButtonClick();
  void SetWaitFullRestoreInit(bool should_wait);

 private:
  void MaybeShowPendingRebootNotification() override;

  void MaybeShowPendingRebootDialog() override;

  PrefService* GetPrefsForActiveProfile() const override;

  const base::Time GetCurrentTime() const override;

  const base::TimeDelta GetSystemUptime() const override;

  void CloseNotifications() override;

  bool ShouldWaitFullRestoreInit() const override;

  int show_dialog_calls_ = 0, show_notification_calls_ = 0,
      close_notification_calls_ = 0;
  bool wait_full_restore_init_ = false;
  const base::Clock* clock_;
  // Default uptime for test is 10h.
  base::TimeDelta uptime_;
  PrefService* prefs_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_REBOOT_NOTIFICATIONS_SCHEDULER_H_
