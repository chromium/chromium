// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TEST_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_TEST_TASK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "chrome/browser/task_manager/task_manager_interface.h"

namespace task_manager {

// This is a partial stub implementation that can be used as a base class for
// implementations of the task manager used in unit tests.
class TestTaskManager : public TaskManagerInterface {
 public:
  TestTaskManager();
  TestTaskManager(const TestTaskManager&) = delete;
  TestTaskManager& operator=(const TestTaskManager&) = delete;
  ~TestTaskManager() override;

  // task_manager::TaskManagerInterface:
  void ActivateTask(TaskId task_id) override;
  bool IsTaskKillable(TaskId task_id) override;
  void KillTask(TaskId task_id) override;
  double GetPlatformIndependentCPUUsage(TaskId task_id) const override;
  base::Time GetStartTime(TaskId task_id) const override;
  base::TimeDelta GetCpuTime(TaskId task_id) const override;
  int64_t GetMemoryFootprintUsage(TaskId task_id) const override;
  int64_t GetSwappedMemoryUsage(TaskId task_id) const override;
  int64_t GetGpuMemoryUsage(TaskId task_id,
                            bool* has_duplicates) const override;
  int GetIdleWakeupsPerSecond(TaskId task_id) const override;
  int GetHardFaultsPerSecond(TaskId task_id) const override;
  int GetNaClDebugStubPort(TaskId task_id) const override;
  void GetGDIHandles(TaskId task_id,
                     int64_t* current,
                     int64_t* peak) const override;
  void GetUSERHandles(TaskId task_id,
                      int64_t* current,
                      int64_t* peak) const override;
  int GetOpenFdCount(TaskId task_id) const override;
  bool IsTaskOnBackgroundedProcess(TaskId task_id) const override;
  const std::u16string& GetTitle(TaskId task_id) const override;
  std::u16string GetProfileName(TaskId task_id) const override;
  const gfx::ImageSkia& GetIcon(TaskId task_id) const override;
  const base::ProcessHandle& GetProcessHandle(TaskId task_id) const override;
  const base::ProcessId& GetProcessId(TaskId task_id) const override;
  Task::Type GetType(TaskId task_id) const override;
  SessionID GetTabId(TaskId task_id) const override;
  int GetChildProcessUniqueId(TaskId task_id) const override;
  void GetTerminationStatus(TaskId task_id,
                            base::TerminationStatus* out_status,
                            int* out_error_code) const override;
  int64_t GetNetworkUsage(TaskId task_id) const override;
  int64_t GetProcessTotalNetworkUsage(TaskId task_id) const override;
  int64_t GetCumulativeNetworkUsage(TaskId task_id) const override;
  int64_t GetCumulativeProcessTotalNetworkUsage(TaskId task_id) const override;
  int64_t GetSqliteMemoryUsed(TaskId task_id) const override;
  bool GetV8Memory(TaskId task_id,
                   int64_t* allocated,
                   int64_t* used) const override;
  bool GetWebCacheStats(TaskId task_id,
                        blink::WebCacheResourceTypeStats* stats) const override;
  int GetKeepaliveCount(TaskId task_id) const override;
  const TaskIdList& GetTaskIdsList() const override;
  TaskIdList GetIdsOfTasksSharingSameProcess(TaskId task_id) const override;
  size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const override;
  bool IsRunningInVM(TaskId task_id) const override;
  TaskId GetTaskIdForWebContents(
      content::WebContents* web_contents) const override;

  base::TimeDelta GetRefreshTime();
  int64_t GetEnabledFlags();

 protected:
  // task_manager::TaskManager:
  void Refresh() override {}
  void StartUpdating() override {}
  void StopUpdating() override {}

  base::ProcessHandle handle_;
  base::ProcessId pid_;
  std::u16string title_;
  gfx::ImageSkia icon_;
  TaskIdList ids_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TEST_TASK_MANAGER_H_
