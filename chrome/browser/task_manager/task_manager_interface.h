// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "components/sessions/core/session_id.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace content {
class WebContents;
}  // namespace content

namespace task_manager {

// Defines the interface for any implementation of the task manager.
// Concrete implementations have no control over the refresh rate nor the
// enabled calculations of the usage of the various resources.
class TaskManagerInterface {
 public:
  // Registers the task manager related prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns true if the user is allowed to end processes.
  static bool IsEndProcessEnabled();

  // Gets the existing instance of the task manager if any, otherwise it will
  // create it first. Must be called on the UI thread.
  static TaskManagerInterface* GetTaskManager();

  void AddObserver(TaskManagerObserver* observer);
  void RemoveObserver(TaskManagerObserver* observer);

  // Activates the task with |task_id| by bringing its container to the front if
  // possible.
  virtual void ActivateTask(TaskId task_id) = 0;

  // Returns if the task is killable.
  virtual bool IsTaskKillable(TaskId task_id) = 0;

  // Kills the task with |task_id|.
  virtual void KillTask(TaskId task_id) = 0;

  // Returns the CPU usage of the process on which |task_id| is running, over
  // the most recent refresh cycle. The value is in the range zero to
  // base::SysInfo::NumberOfProcessors() * 100%.
  virtual double GetPlatformIndependentCPUUsage(TaskId task_id) const = 0;

  // Returns the start time for the process on which the task
  // with |task_id| is running. Only implemented in Windows now.
  virtual base::Time GetStartTime(TaskId task_id) const = 0;

  // Returns the CPU time for the process on which the task
  // with |task_id| is running during the current refresh cycle.
  // Only implemented in Windows now.
  virtual base::TimeDelta GetCpuTime(TaskId task_id) const = 0;

  // Returns the current memory footprint/swapped memory of the task with
  // |task_id| in bytes. A value of -1 means no valid value is currently
  // available.
  virtual int64_t GetMemoryFootprintUsage(TaskId task_id) const = 0;
  virtual int64_t GetSwappedMemoryUsage(TaskId task_id) const = 0;

  // Returns the GPU memory usage of the task with |task_id| in bytes. A value
  // of -1 means no valid value is currently available.
  // |has_duplicates| will be set to true if this process' GPU resource count is
  // inflated because it is counting other processes' resources.
  virtual int64_t GetGpuMemoryUsage(TaskId task_id,
                                    bool* has_duplicates) const = 0;

  // Returns the number of average idle CPU wakeups per second since the last
  // refresh cycle. A value of -1 means no valid value is currently available.
  virtual int GetIdleWakeupsPerSecond(TaskId task_id) const = 0;

  // Returns the number of hard page faults per second since the last refresh
  // cycle. A value of -1 means no valid value is currently available.
  virtual int GetHardFaultsPerSecond(TaskId task_id) const = 0;

  // Returns the NaCl GDB debug stub port. A value of
  // |nacl::kGdbDebugStubPortUnknown| means no valid value is currently
  // available. A value of -2 means NaCl is not enabled for this build.
  virtual int GetNaClDebugStubPort(TaskId task_id) const = 0;

  // On Windows, gets the current and peak number of GDI and USER handles in
  // use. A value of -1 means no valid value is currently available.
  virtual void GetGDIHandles(TaskId task_id,
                             int64_t* current,
                             int64_t* peak) const = 0;
  virtual void GetUSERHandles(TaskId task_id,
                              int64_t* current,
                              int64_t* peak) const = 0;

  // On Linux and ChromeOS, gets the number of file descriptors currently open
  // by the process on which the task with |task_id| is running, or -1 if no
  // valid value is currently available.
  virtual int GetOpenFdCount(TaskId task_id) const = 0;

  // Returns whether the task with |task_id| is running on a backgrounded
  // process.
  virtual bool IsTaskOnBackgroundedProcess(TaskId task_id) const = 0;

  // Returns the title of the task with |task_id|.
  virtual const base::string16& GetTitle(TaskId task_id) const = 0;

  // Returns the canonicalized name of the task with |task_id| that can be used
  // to represent this task in a Rappor sample via RapporServiceImpl.
  virtual const std::string& GetTaskNameForRappor(TaskId task_id) const = 0;

  // Returns the name of the profile associated with the browser context of the
  // render view host that the task with |task_id| represents (if that task
  // represents a renderer).
  virtual base::string16 GetProfileName(TaskId task_id) const = 0;

  // Returns the favicon of the task with |task_id|.
  virtual const gfx::ImageSkia& GetIcon(TaskId task_id) const = 0;

  // Returns the ID and handle of the process on which the task with |task_id|
  // is running.
  virtual const base::ProcessHandle& GetProcessHandle(TaskId task_id) const = 0;
  virtual const base::ProcessId& GetProcessId(TaskId task_id) const = 0;

  // Returns the type of the task with |task_id|.
  virtual Task::Type GetType(TaskId task_id) const = 0;

  // Gets the unique ID of the tab if the task with |task_id| represents a
  // WebContents of a tab. Returns -1 otherwise.
  virtual SessionID GetTabId(TaskId task_id) const = 0;

  // Returns the unique ID of the BrowserChildProcessHost/RenderProcessHost on
  // which the task with |task_id| is running. It is not the PID nor the handle
  // of the process.
  // For a task that represents the browser process, the return value is 0.
  // For a task that doesn't have a host on the browser process, the return
  // value is also 0. For other tasks that represent renderers and other child
  // processes, the return value is whatever unique IDs of their hosts in the
  // browser process.
  virtual int GetChildProcessUniqueId(TaskId task_id) const = 0;

  // If the process, in which the task with |task_id| is running, is terminated
  // this gets the termination status. Currently implemented only for Renderer
  // processes. The values will only be valid if the process has actually
  // terminated.
  virtual void GetTerminationStatus(TaskId task_id,
                                    base::TerminationStatus* out_status,
                                    int* out_error_code) const = 0;

  // Returns the network usage (in bytes per second) during the current refresh
  // cycle for the task with |task_id|.
  virtual int64_t GetNetworkUsage(TaskId task_id) const = 0;

  // Returns the network usage during the current lifetime of the task
  // for the task with |task_id|.
  virtual int64_t GetCumulativeNetworkUsage(TaskId task_id) const = 0;

  // Returns the total network usage (in bytes per second) during the current
  // refresh cycle for the process on which the task with |task_id| is running.
  // This is the sum of all the network usage of the individual tasks (that
  // can be gotten by the above GetNetworkUsage()). A value of -1 means network
  // usage calculation refresh is currently not available.
  virtual int64_t GetProcessTotalNetworkUsage(TaskId task_id) const = 0;

  // Returns the total network usage during the lifetime of the process
  // on which the task with |task_id| is running.
  // This is the sum of all the network usage of the individual tasks (that
  // can be gotten by the above GetTotalNetworkUsage()).
  virtual int64_t GetCumulativeProcessTotalNetworkUsage(
      TaskId task_id) const = 0;

  // Returns the Sqlite used memory (in bytes) for the task with |task_id|.
  // A value of -1 means no valid value is currently available.
  virtual int64_t GetSqliteMemoryUsed(TaskId task_id) const = 0;

  // Returns the allocated and used V8 memory (in bytes) for the task with
  // |task_id|. A return value of false means no valid value is currently
  // available.
  virtual bool GetV8Memory(TaskId task_id,
                           int64_t* allocated,
                           int64_t* used) const = 0;

  // Gets the Blink resource cache stats for the task with |task_id|.
  // A return value of false means that task does NOT report WebCache stats.
  virtual bool GetWebCacheStats(
      TaskId task_id,
      blink::WebCacheResourceTypeStats* stats) const = 0;

  // Returns the keep-alive counter if the Task is an event page, -1 otherwise.
  virtual int GetKeepaliveCount(TaskId task_id) const = 0;

  // Gets the list of task IDs currently tracked by the task manager. Tasks that
  // share the same process id will always be consecutive. The list will be
  // sorted in a way that reflects the process tree: the browser process will be
  // first, followed by the gpu process if it exists. Related processes (e.g., a
  // subframe process and its parent) will be kept together if possible. Callers
  // can expect this ordering to be stable when a process is added or removed.
  virtual const TaskIdList& GetTaskIdsList() const = 0;

  // Gets the list of task IDs of the tasks that run on the same process as the
  // task with |task_id|. The returned list will at least include |task_id|.
  virtual TaskIdList GetIdsOfTasksSharingSameProcess(TaskId task_id) const = 0;

  // Gets the number of task-manager tasks running on the same process on which
  // the Task with |task_id| is running.
  virtual size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const = 0;

  // Returns true if the task is running inside a VM.
  virtual bool IsRunningInVM(TaskId task_id) const = 0;

  // Returns the TaskId associated with the main task for |web_contents|.
  // Returns -1 if |web_contents| is nullptr or does not currently have an
  // associated Task.
  virtual TaskId GetTaskIdForWebContents(
      content::WebContents* web_contents) const = 0;

  // Returns true if the resource |type| usage calculation is enabled and
  // the implementation should refresh its value (this means that at least one
  // of the observers require this value). False otherwise.
  bool IsResourceRefreshEnabled(RefreshType type) const;

 protected:
  TaskManagerInterface();
  virtual ~TaskManagerInterface();

  // Notifying observers of various events.
  void NotifyObserversOnTaskAdded(TaskId id);
  void NotifyObserversOnTaskToBeRemoved(TaskId id);
  void NotifyObserversOnRefresh(const TaskIdList& task_ids);
  void NotifyObserversOnRefreshWithBackgroundCalculations(
      const TaskIdList& task_ids);
  void NotifyObserversOnTaskUnresponsive(TaskId id);

  // Refresh all the enabled resources usage of all the available tasks.
  virtual void Refresh() = 0;

  // StartUpdating will be called once an observer is added, and StopUpdating
  // will be called when the last observer is removed.
  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

  // Returns the current refresh time that this task manager is running at. It
  // will return base::TimeDelta::Max() if the task manager is not running.
  base::TimeDelta GetCurrentRefreshTime() const;

  int64_t enabled_resources_flags() const { return enabled_resources_flags_; }

  void set_timer_for_testing(std::unique_ptr<base::RepeatingTimer> timer) {
    refresh_timer_ = std::move(timer);
  }

 private:
  friend class TaskManagerObserver;

  // This should be called after each time an observer changes its own desired
  // resources refresh flags.
  void RecalculateRefreshFlags();

  // Appends |flags| to the |enabled_resources_flags_|.
  void ResourceFlagsAdded(int64_t flags);

  // Sets |enabled_resources_flags_| to |flags|.
  void SetEnabledResourceFlags(int64_t flags);

  // Schedules the task manager refresh cycles using the given |refresh_time|.
  // It stops any existing refresh schedule.
  void ScheduleRefresh(base::TimeDelta refresh_time);

  // The list of observers.
  base::ObserverList<TaskManagerObserver>::Unchecked observers_;

  // The timer that will be used to schedule the successive refreshes.
  std::unique_ptr<base::RepeatingTimer> refresh_timer_;

  // The flags containing the enabled resources types calculations.
  int64_t enabled_resources_flags_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerInterface);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_INTERFACE_H_
