// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_NOTIFIER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace chromeos {

// Schedules warning notifications for screen time usage or bed time limits.
class TimeLimitNotifier {
 public:
  // The types of time limits to notify for.
  enum class LimitType { kScreenTime, kBedTime, kOverride };

  explicit TimeLimitNotifier(content::BrowserContext* context);
  ~TimeLimitNotifier();

  // Schedules warning and/or exit notifications based on the time remaining.
  void MaybeScheduleLockNotifications(LimitType limit_type,
                                      base::TimeDelta remaining_time);

  // Shows a notification informing that the provided limit was updated.
  void ShowPolicyUpdateNotification(LimitType limit_type,
                                    base::Optional<base::Time> lock_time);

  // Cancels any scheduled notification timers.
  void UnscheduleNotifications();

 private:
  friend class TimeLimitNotifierTest;

  // For tests, sets up the notification timers using the given task runner.
  TimeLimitNotifier(content::BrowserContext* context,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  content::BrowserContext* const context_;

  // Called to show warning and exit notifications.
  base::OneShotTimer warning_notification_timer_;
  base::OneShotTimer exit_notification_timer_;

  DISALLOW_COPY_AND_ASSIGN(TimeLimitNotifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_NOTIFIER_H_
