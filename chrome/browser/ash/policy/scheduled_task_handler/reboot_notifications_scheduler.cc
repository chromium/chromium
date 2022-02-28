// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"

#include "base/system/sys_info.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"

namespace policy {

namespace {
constexpr base::TimeDelta kNotificationDelay = base::Hours(1);
constexpr base::TimeDelta kDialogDelay = base::Minutes(5);
constexpr base::TimeDelta kGraceTime = base::Hours(1);
}  // namespace

RebootNotificationsScheduler::RebootNotificationsScheduler()
    : RebootNotificationsScheduler(base::DefaultClock::GetInstance(),
                                   base::DefaultTickClock::GetInstance()) {}

RebootNotificationsScheduler::RebootNotificationsScheduler(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : notification_timer_(clock, tick_clock),
      dialog_timer_(clock, tick_clock) {}

RebootNotificationsScheduler::~RebootNotificationsScheduler() = default;

void RebootNotificationsScheduler::ScheduleNotifications(
    base::OnceClosure reboot_callback,
    const base::Time& reboot_time) {
  ResetState();
  if (ShouldApplyGraceTime(reboot_time)) {
    return;
  }

  reboot_time_ = reboot_time;
  reboot_callback_ = std::move(reboot_callback);
  base::TimeDelta delay = GetRebootDelay(reboot_time_);

  if (delay > kNotificationDelay) {
    base::Time timer_run_time = reboot_time_ - kNotificationDelay;
    notification_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(&RebootNotificationsScheduler::MaybeShowNotification,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    MaybeShowNotification();
  }

  if (delay > kDialogDelay) {
    base::Time timer_run_time = reboot_time_ - kDialogDelay;
    dialog_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(&RebootNotificationsScheduler::MaybeShowDialog,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  MaybeShowDialog();
}

void RebootNotificationsScheduler::ResetState() {
  if (notification_timer_.IsRunning())
    notification_timer_.Stop();
  if (dialog_timer_.IsRunning())
    dialog_timer_.Stop();
  notification_controller_.CloseRebootNotification();
  notification_controller_.CloseRebootDialog();
  reboot_callback_.Reset();
}

bool RebootNotificationsScheduler::ShouldApplyGraceTime(
    const base::Time& reboot_time) const {
  base::TimeDelta delay = GetRebootDelay(reboot_time);
  return ((delay + GetSystemUptime()) <= kGraceTime);
}

void RebootNotificationsScheduler::MaybeShowNotification() {
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time_,
      base::BindRepeating(&RebootNotificationsScheduler::OnRebootButtonClicked,
                          base::Unretained(this)));
}

void RebootNotificationsScheduler::MaybeShowDialog() {
  notification_controller_.MaybeShowPendingRebootDialog(
      reboot_time_,
      base::BindOnce(&RebootNotificationsScheduler::OnRebootButtonClicked,
                     base::Unretained(this)));
}

void RebootNotificationsScheduler::OnRebootButtonClicked() {
  DCHECK(reboot_callback_);
  std::move(reboot_callback_).Run();
}

const base::Time RebootNotificationsScheduler::GetCurrentTime() const {
  return base::Time::Now();
}

const base::TimeDelta RebootNotificationsScheduler::GetSystemUptime() const {
  return base::SysInfo::Uptime();
}

base::TimeDelta RebootNotificationsScheduler::GetRebootDelay(
    const base::Time& reboot_time) const {
  return (reboot_time - GetCurrentTime());
}

}  // namespace policy