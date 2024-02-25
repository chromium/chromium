// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TASK_SWITCH_TIME_TRACKER_H_
#define ASH_METRICS_TASK_SWITCH_TIME_TRACKER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace base {
class HistogramBase;
class TickClock;
}

namespace ash {

// Tracks time deltas between task switches and records them in a histogram.
class ASH_EXPORT TaskSwitchTimeTracker {
 public:
  // Create a TaskSwitchTimeTracker that will record data to the histogram with
  // the given |histogram_name|.
  explicit TaskSwitchTimeTracker(const std::string& histogram_name);

  TaskSwitchTimeTracker(const TaskSwitchTimeTracker&) = delete;
  TaskSwitchTimeTracker& operator=(const TaskSwitchTimeTracker&) = delete;

  ~TaskSwitchTimeTracker();

  // Notifies |this| that a task switch has occurred. A histogram data point
  // will be recorded for all calls but the first.
  void OnTaskSwitch();

 private:
  friend class TaskSwitchTimeTrackerTestAPI;

  // Private constructor that the TaskSwitchTimeTrackerTestAPI can use to
  // inject a custom |tick_clock|.
  // This doesn't take the ownership of the clock. |tick_clock| must outlive
  // TaskSwitchTimeTracker.
  TaskSwitchTimeTracker(const std::string& histogram_name,
                        const base::TickClock* tick_clock);

  // Returns true if |last_action_time_| has a valid value.
  bool HasLastActionTime() const;

  // Sets the |last_action_time_| to |tick_clock_|'s current value and returns
  // the previous value for |last_action_time_|.
  base::TimeTicks SetLastActionTime();

  // Records a data point in the histogram.
  void RecordTimeDelta();

  // Lazily obtains and sets the |histogram_|.
  base::HistogramBase* GetHistogram();

  // The histogram name to record data to.
  std::string histogram_name_;

  // The histogram to log data to. Set via GetHistogram() using lazy load.
  raw_ptr<base::HistogramBase> histogram_ = nullptr;

  // Tracks the last time OnTaskSwitch() was called. A value of
  // base::TimeTicks() should be interpreted as not set.
  base::TimeTicks last_action_time_ = base::TimeTicks();

  // The clock used to determine the |last_action_time_|.
  raw_ptr<const base::TickClock> tick_clock_;
};

}  // namespace ash

#endif  // ASH_METRICS_TASK_SWITCH_TIME_TRACKER_H_
