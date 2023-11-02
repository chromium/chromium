// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TASK_SWITCH_TIME_TRACKER_TEST_API_H_
#define ASH_METRICS_TASK_SWITCH_TIME_TRACKER_TEST_API_H_

#include <memory>
#include <string>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"

namespace ash {

class TaskSwitchTimeTracker;

// Provides access TaskSwitchTimeTracker's internals.
class TaskSwitchTimeTrackerTestAPI {
 public:
  // Creates a TaskSwitchTimeTracker with the given |histogram_name| and injects
  // a base::SimpleTestTickClock that can be controlled.
  explicit TaskSwitchTimeTrackerTestAPI(const std::string& histogram_name);

  TaskSwitchTimeTrackerTestAPI(const TaskSwitchTimeTrackerTestAPI&) = delete;
  TaskSwitchTimeTrackerTestAPI& operator=(const TaskSwitchTimeTrackerTestAPI&) =
      delete;

  ~TaskSwitchTimeTrackerTestAPI();

  TaskSwitchTimeTracker* time_tracker() { return time_tracker_.get(); }

  // Advances |time_tracker_|'s TickClock by |time_delta|.
  void Advance(base::TimeDelta time_delta);

  // Wrapper function to access TaskSwitchTimeTracker::HasLastActionTime();
  bool HasLastActionTime() const;

 private:
  base::SimpleTestTickClock tick_clock_;

  // The TaskSwitchTimeTracker to provide internal access to.
  std::unique_ptr<TaskSwitchTimeTracker> time_tracker_;
};

}  // namespace ash

#endif  // ASH_METRICS_TASK_SWITCH_TIME_TRACKER_TEST_API_H_
