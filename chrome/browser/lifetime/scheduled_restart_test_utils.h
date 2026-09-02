// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_TEST_UTILS_H_
#define CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_TEST_UTILS_H_

#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"

namespace scheduled_restart {

// A fake UpgradeDetector scoped specifically for Scheduled Restart tests,
// providing control over upgrade detection state, detection timestamps, and
// notification stages.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  FakeUpgradeDetector();
  FakeUpgradeDetector(const base::Clock* clock,
                      const base::TickClock* tick_clock);

  FakeUpgradeDetector(const FakeUpgradeDetector&) = delete;
  FakeUpgradeDetector& operator=(const FakeUpgradeDetector&) = delete;

  ~FakeUpgradeDetector() override;

  // UpgradeDetector:
  // Returns the timestamp when `level` will be reached. Returns a null
  // base::Time() as deadline calculation is unused in Scheduled Restart tests.
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override;

  using UpgradeDetector::NotifyUpgrade;
  using UpgradeDetector::set_upgrade_available;
  using UpgradeDetector::set_upgrade_detected_time;
  using UpgradeDetector::set_upgrade_notification_stage;

  // Convenience helper that marks an upgrade as available with low annoyance
  // level and immediately notifies observers.
  void SetUpgradeAvailable();
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_TEST_UTILS_H_
