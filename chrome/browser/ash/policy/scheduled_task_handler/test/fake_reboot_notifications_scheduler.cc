// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"

#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/prefs/pref_service.h"

namespace policy {

FakeRebootNotificationsScheduler::FakeRebootNotificationsScheduler(
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    PrefService* prefs)
    : RebootNotificationsScheduler(clock, tick_clock), prefs_(prefs) {}

FakeRebootNotificationsScheduler::~FakeRebootNotificationsScheduler() = default;

int FakeRebootNotificationsScheduler::GetShowDialogCalls() const {
  return show_dialog_calls_;
}
int FakeRebootNotificationsScheduler::GetShowNotificationCalls() const {
  return show_notification_calls_;
}
int FakeRebootNotificationsScheduler::GetCloseNotificationCalls() const {
  return close_notification_calls_;
}

void FakeRebootNotificationsScheduler::SimulateRebootButtonClick() {
  OnRebootButtonClicked();
}

void FakeRebootNotificationsScheduler::SetWaitFullRestoreInit(
    bool should_wait) {
  wait_full_restore_init_ = should_wait;
}

void FakeRebootNotificationsScheduler::MaybeShowPendingRebootNotification() {
  ++show_notification_calls_;
}

void FakeRebootNotificationsScheduler::MaybeShowPendingRebootDialog() {
  ++show_dialog_calls_;
}

PrefService* FakeRebootNotificationsScheduler::GetPrefsForActiveProfile()
    const {
  return prefs_;
}

void FakeRebootNotificationsScheduler::CloseNotifications() {
  ++close_notification_calls_;
}

bool FakeRebootNotificationsScheduler::ShouldWaitFullRestoreInit() const {
  return wait_full_restore_init_;
}

}  // namespace policy
