// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_APP_MODE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_

namespace ash {

// `RepeatingTimeIntervalTaskExecutor` takes a time interval and a set of
// repeating callbacks. When the device enters and exits the specified time
// interval, this class invokes the provided start and end callbacks. This class
// schedules the time interval using the system timezone. Changes to the system
// timezone will make it reprogram the time interval.
// TODO(b/319083748) Add constructor to accept intervals and callbacks.
// TODO(b/319087271) Implement case when current time falls in interval.
// TODO(b/319086751) Implement case when next interval is in the future.
// TODO(b/319083880) Observe time zone changes and cancel pending executors.
class RepeatingTimeIntervalTaskExecutor {
 public:
  RepeatingTimeIntervalTaskExecutor();

  RepeatingTimeIntervalTaskExecutor(const RepeatingTimeIntervalTaskExecutor&) =
      delete;
  RepeatingTimeIntervalTaskExecutor& operator=(
      const RepeatingTimeIntervalTaskExecutor&) = delete;

  ~RepeatingTimeIntervalTaskExecutor();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
