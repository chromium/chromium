// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"

#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"

namespace scheduled_restart {

FakeUpgradeDetector::FakeUpgradeDetector()
    : UpgradeDetector(base::DefaultClock::GetInstance(),
                      base::DefaultTickClock::GetInstance()) {
  set_upgrade_detected_time(this->clock()->Now());
}

FakeUpgradeDetector::FakeUpgradeDetector(const base::Clock* clock,
                                         const base::TickClock* tick_clock)
    : UpgradeDetector(clock, tick_clock) {
  set_upgrade_detected_time(this->clock()->Now());
}

FakeUpgradeDetector::~FakeUpgradeDetector() = default;

base::Time FakeUpgradeDetector::GetAnnoyanceLevelDeadline(
    UpgradeNotificationAnnoyanceLevel /*level*/) {
  return base::Time();
}

void FakeUpgradeDetector::SetUpgradeAvailable() {
  set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);
  set_upgrade_available(UPGRADE_AVAILABLE_REGULAR);
  NotifyUpgrade();
}

}  // namespace scheduled_restart
