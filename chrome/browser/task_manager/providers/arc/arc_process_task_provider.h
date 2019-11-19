// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "chrome/browser/task_manager/providers/arc/arc_process_task.h"
#include "chrome/browser/task_manager/providers/task_provider.h"

namespace task_manager {

// This provides the ARC process tasks.
//
// Since this provider obtains ARC process information via IPC and procfs,
// it can never avoid race conditions. For example, in an extreme case such as
// fork(2) is called millions of times in a second, this provider can return
// wrong results. However, its chance is very low, and even if we hit the case,
// the worst outcome is just that an app (non-system) process which
// the user did not intend to choose is killed. Since apps are designed
// to be killed at any time, it sounds acceptable.
class ArcProcessTaskProvider : public TaskProvider {
 public:
  ArcProcessTaskProvider();
  ~ArcProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  using ArcTaskMap =
      std::unordered_map<base::ProcessId, std::unique_ptr<ArcProcessTask>>;
  using OptionalArcProcessList = arc::ArcProcessService::OptionalArcProcessList;
  void ScheduleNextRequest(const base::Closure& task, const int delaySeconds);

  // Auto-retry if ARC bridge service is not ready.
  void RequestAppProcessList();
  void RequestSystemProcessList();

  void UpdateProcessList(ArcTaskMap* pid_to_task,
                         std::vector<arc::ArcProcess> processes);
  void OnUpdateAppProcessList(OptionalArcProcessList processes);
  void OnUpdateSystemProcessList(OptionalArcProcessList processes);

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  void ScheduleNextAppRequest();
  void ScheduleNextSystemRequest();

  ArcTaskMap nspid_to_task_;
  ArcTaskMap nspid_to_sys_task_;

  // Whether to continue the periodical polling.
  bool is_updating_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcProcessTaskProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcProcessTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
