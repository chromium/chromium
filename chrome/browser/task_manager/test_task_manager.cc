// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/test_task_manager.h"

#include "base/timer/mock_timer.h"

namespace task_manager {

TestTaskManager::TestTaskManager()
    : handle_(base::kNullProcessHandle),
      pid_(base::kNullProcessId) {
  set_timer_for_testing(std::make_unique<base::MockRepeatingTimer>());
}

TestTaskManager::~TestTaskManager() {
}

void TestTaskManager::ActivateTask(TaskId task_id) {
}

bool TestTaskManager::IsTaskKillable(TaskId task_id) {
  return true;
}

void TestTaskManager::KillTask(TaskId task_id) {
}

double TestTaskManager::GetPlatformIndependentCPUUsage(TaskId task_id) const {
  return 0.0;
}

base::Time TestTaskManager::GetStartTime(TaskId task_id) const {
  return base::Time();
}

base::TimeDelta TestTaskManager::GetCpuTime(TaskId task_id) const {
  return base::TimeDelta();
}

int64_t TestTaskManager::GetMemoryFootprintUsage(TaskId task_id) const {
  return -1;
}

int64_t TestTaskManager::GetSwappedMemoryUsage(TaskId task_id) const {
  return -1;
}

int64_t TestTaskManager::GetGpuMemoryUsage(TaskId task_id,
                                           bool* has_duplicates) const {
  return -1;
}

int TestTaskManager::GetIdleWakeupsPerSecond(TaskId task_id) const {
  return -1;
}

int TestTaskManager::GetHardFaultsPerSecond(TaskId task_id) const {
  return -1;
}

int TestTaskManager::GetNaClDebugStubPort(TaskId task_id) const {
  return -1;
}

void TestTaskManager::GetGDIHandles(TaskId task_id,
                                    int64_t* current,
                                    int64_t* peak) const {}

void TestTaskManager::GetUSERHandles(TaskId task_id,
                                     int64_t* current,
                                     int64_t* peak) const {}

int TestTaskManager::GetOpenFdCount(TaskId task_id) const {
  return -1;
}

bool TestTaskManager::IsTaskOnBackgroundedProcess(TaskId task_id) const {
  return false;
}

const std::u16string& TestTaskManager::GetTitle(TaskId task_id) const {
  return title_;
}

std::u16string TestTaskManager::GetProfileName(TaskId task_id) const {
  return std::u16string();
}

const gfx::ImageSkia& TestTaskManager::GetIcon(TaskId task_id) const {
  return icon_;
}

const base::ProcessHandle& TestTaskManager::GetProcessHandle(
    TaskId task_id) const {
  return handle_;
}

const base::ProcessId& TestTaskManager::GetProcessId(TaskId task_id) const {
  return pid_;
}

Task::Type TestTaskManager::GetType(TaskId task_id) const {
  return Task::UNKNOWN;
}

SessionID TestTaskManager::GetTabId(TaskId task_id) const {
  return SessionID::InvalidValue();
}

int TestTaskManager::GetChildProcessUniqueId(TaskId task_id) const {
  return 0;
}

void TestTaskManager::GetTerminationStatus(TaskId task_id,
                                           base::TerminationStatus* out_status,
                                           int* out_error_code) const {
  DCHECK(out_status);
  DCHECK(out_error_code);

  *out_status = base::TERMINATION_STATUS_STILL_RUNNING;
  *out_error_code = 0;
}

int64_t TestTaskManager::GetNetworkUsage(TaskId task_id) const {
  return 0;
}

int64_t TestTaskManager::GetProcessTotalNetworkUsage(TaskId task_id) const {
  return -1;
}

int64_t TestTaskManager::GetCumulativeNetworkUsage(TaskId task_id) const {
  return 0;
}

int64_t TestTaskManager::GetCumulativeProcessTotalNetworkUsage(
    TaskId task_id) const {
  return 0;
}

int64_t TestTaskManager::GetSqliteMemoryUsed(TaskId task_id) const {
  return -1;
}

bool TestTaskManager::GetV8Memory(TaskId task_id,
                                  int64_t* allocated,
                                  int64_t* used) const {
  return false;
}

bool TestTaskManager::GetWebCacheStats(
    TaskId task_id,
    blink::WebCacheResourceTypeStats* stats) const {
  return false;
}

int TestTaskManager::GetKeepaliveCount(TaskId task_id) const {
  return -1;
}

const TaskIdList& TestTaskManager::GetTaskIdsList() const {
  return ids_;
}

TaskIdList TestTaskManager::GetIdsOfTasksSharingSameProcess(
    TaskId task_id) const {
  TaskIdList result;
  result.push_back(task_id);
  return result;
}

size_t TestTaskManager::GetNumberOfTasksOnSameProcess(TaskId task_id) const {
  return 1;
}

bool TestTaskManager::IsRunningInVM(TaskId task_id) const {
  return false;
}

TaskId TestTaskManager::GetTaskIdForWebContents(
    content::WebContents* web_contents) const {
  return -1;
}

base::TimeDelta TestTaskManager::GetRefreshTime() {
  return GetCurrentRefreshTime();
}

int64_t TestTaskManager::GetEnabledFlags() {
  return enabled_resources_flags();
}

}  // namespace task_manager
