// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_OBSERVER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace task_manager {

class TaskManagerInterface;

using TaskId = int64_t;
using TaskIdList = std::vector<TaskId>;

// Defines a list of types of resources that an observer needs to be refreshed
// on every task manager refresh cycle.
// Note: task_manager.mojom API SetRefreshFlags passes the bit flags
// (|refresh_flags|) of RefreshType as an argument, which requires that
// RefreshType must be stablized. Therefore, we can never reorder or delete
// old types.
enum RefreshType {
  REFRESH_TYPE_NONE = 0,
  REFRESH_TYPE_CPU = 1,

  // Only available on CrOS.
  REFRESH_TYPE_SWAPPED_MEM = 1 << 2,
  REFRESH_TYPE_GPU_MEMORY = 1 << 3,
  REFRESH_TYPE_V8_MEMORY = 1 << 4,
  REFRESH_TYPE_SQLITE_MEMORY = 1 << 5,
  REFRESH_TYPE_WEBCACHE_STATS = 1 << 6,
  REFRESH_TYPE_NETWORK_USAGE = 1 << 7,
  REFRESH_TYPE_NACL = 1 << 8,
  REFRESH_TYPE_IDLE_WAKEUPS = 1 << 9,
  REFRESH_TYPE_HANDLES = 1 << 10,
  REFRESH_TYPE_START_TIME = 1 << 11,
  REFRESH_TYPE_CPU_TIME = 1 << 12,

  // Whether an observer is interested in knowing if a process is foregrounded
  // or backgrounded.
  REFRESH_TYPE_PRIORITY = 1 << 13,

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  // For observers interested in getting the number of open file descriptors of
  // processes.
  REFRESH_TYPE_FD_COUNT = 1 << 14,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

  REFRESH_TYPE_KEEPALIVE_COUNT = 1 << 15,
  REFRESH_TYPE_MEMORY_FOOTPRINT = 1 << 16,
  REFRESH_TYPE_HARD_FAULTS = 1 << 17,
};

// Defines the interface for observers of the task manager.
class TaskManagerObserver {
 public:
  static bool IsResourceRefreshEnabled(RefreshType refresh_type,
                                       int refresh_flags);

  // Constructs a TaskManagerObserver given the minimum |refresh_time| that it
  // it requires the task manager to be refreshing the values at, along with the
  // |resources_flags| that it needs to be calculated on each refresh cycle of
  // the task manager (use the above flags in |ResourceType|).
  //
  // Notes:
  // 1- The task manager will refresh at least once every |refresh_time| as
  // long as this observer is added to it. There might be other observers that
  // require more frequent refreshes.
  // 2- Refresh time values less than 1 second will be considered as 1 second.
  // 3- Depending on the other observers, the task manager may refresh more
  // resources than those defined in |resources_flags|.
  // 4- Upon the removal of the observer from the task manager, the task manager
  // will update its refresh time and the calculated resources to be the minimum
  // required value of all the remaining observers.
  TaskManagerObserver(base::TimeDelta refresh_time, int64_t resources_flags);
  TaskManagerObserver(const TaskManagerObserver&) = delete;
  TaskManagerObserver& operator=(const TaskManagerObserver&) = delete;
  virtual ~TaskManagerObserver();

  // Notifies the observer that a chrome task with |id| has started and the task
  // manager is now monitoring it. The resource usage of this newly-added task
  // will remain invalid until the next refresh cycle of the task manager.
  virtual void OnTaskAdded(TaskId id) {}

  // Notifies the observer that a chrome task with |id| is about to be destroyed
  // and removed from the task manager right after this call. Observers which
  // are interested in doing some calculations related to the resource usage of
  // this task upon its removal may do so inside this call.
  virtual void OnTaskToBeRemoved(TaskId id) {}

  // Notifies the observer that the task manager has just finished a refresh
  // cycle to calculate the resources usage of all tasks whose IDs are given in
  // |task_ids|. |task_ids| will be sorted such that the task representing the
  // browser process is at the top of the list and the rest of the IDs will be
  // sorted by the process IDs on which the tasks are running, then by the task
  // IDs themselves.
  virtual void OnTasksRefreshed(const TaskIdList& task_ids) {}

  // Notifies the observer that the task manager has just finished a refresh
  // cycle that calculated all the resource usage of all tasks whose IDs are in
  // |task_ids| including the resource usages that are calculated in the
  // background such CPU and memory (If those refresh types are enabled).
  // This event can take longer to be fired, and can miss some changes that may
  // happen to non-background calculations in-between two successive
  // invocations. Listen to this ONLY if you must know when all the background
  // resource calculations to be valid for all the available processes.
  // |task_ids| will be sorted as specified in OnTasksRefreshed() above.
  virtual void OnTasksRefreshedWithBackgroundCalculations(
      const TaskIdList& task_ids) {}

  // Notifies the observer that the task with |id| is running on a renderer that
  // has become unresponsive.
  virtual void OnTaskUnresponsive(TaskId id) {}

  virtual void OnActiveTaskFetched(TaskId id) {}

  const base::TimeDelta& desired_refresh_time() const {
    return desired_refresh_time_;
  }

  int64_t desired_resources_flags() const { return desired_resources_flags_; }

 protected:
  TaskManagerInterface* observed_task_manager() const {
    return observed_task_manager_;
  }

  // Add or Remove a refresh |type|.
  void AddRefreshType(RefreshType type);
  void RemoveRefreshType(RefreshType type);
  void SetRefreshTypesFlags(int64_t flags);

 private:
  friend class TaskManagerInterface;

  // The currently observed task Manager.
  raw_ptr<TaskManagerInterface> observed_task_manager_;

  // The minimum update time of the task manager that this observer needs to
  // do its job.
  base::TimeDelta desired_refresh_time_;

  // The flags that contain the resources that this observer needs to be
  // calculated on each refresh.
  int64_t desired_resources_flags_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_OBSERVER_H_
