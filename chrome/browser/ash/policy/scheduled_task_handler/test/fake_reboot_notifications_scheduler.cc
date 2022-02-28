// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"

#include <memory>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

FakeRebootNotificationsScheduler::FakeRebootNotificationsScheduler(
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    std::unique_ptr<icu::TimeZone> time_zone)
    : RebootNotificationsScheduler(clock, tick_clock),
      clock_(clock),
      time_zone_(std::move(time_zone)),
      uptime_(base::Hours(10)) {}

FakeRebootNotificationsScheduler::~FakeRebootNotificationsScheduler() = default;

int FakeRebootNotificationsScheduler::GetShowDialogCalls() const {
  return show_dialog_calls_;
}
int FakeRebootNotificationsScheduler::GetShowNotificationCalls() const {
  return show_notification_calls_;
}
void FakeRebootNotificationsScheduler::SetUptime(base::TimeDelta uptime) {
  uptime_ = uptime;
}

void FakeRebootNotificationsScheduler::SimulateRebootButtonClick() {
  OnRebootButtonClicked();
}

void FakeRebootNotificationsScheduler::MaybeShowNotification() {
  ++show_notification_calls_;
}

void FakeRebootNotificationsScheduler::MaybeShowDialog() {
  ++show_dialog_calls_;
}

const base::Time FakeRebootNotificationsScheduler::GetCurrentTime() const {
  return clock_->Now();
}

const icu::TimeZone& FakeRebootNotificationsScheduler::GetTimeZone() const {
  return *time_zone_;
}

const base::TimeDelta FakeRebootNotificationsScheduler::GetSystemUptime()
    const {
  return uptime_;
}
}  // namespace policy
