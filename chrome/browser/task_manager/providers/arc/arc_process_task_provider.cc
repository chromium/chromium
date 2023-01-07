// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/arc/arc_process_task_provider.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/process.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"

namespace task_manager {

using std::set;
using arc::ArcProcess;
using base::Process;
using base::ProcessId;

ArcProcessTaskProvider::ArcProcessTaskProvider() : is_updating_(false) {}

ArcProcessTaskProvider::~ArcProcessTaskProvider() = default;

Task* ArcProcessTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  // ARC tasks are not associated with any URL request.
  return nullptr;
}

void ArcProcessTaskProvider::UpdateProcessList(
    ArcTaskMap* pid_to_task,
    std::vector<ArcProcess> processes) {
  if (!is_updating_)
    return;

  // NB: |processes| can be already stale here because it is sent via IPC, and
  // we can never avoid that. See also the comment at the declaration of
  // ArcProcessTaskProvider.

  set<ProcessId> nspid_to_remove;
  for (const auto& entry : *pid_to_task)
    nspid_to_remove.insert(entry.first);

  for (auto& entry : processes) {
    if (nspid_to_remove.erase(entry.nspid()) == 0) {
      // New arc process.
      std::unique_ptr<ArcProcessTask>& task = (*pid_to_task)[entry.nspid()];
      // After calling NotifyObserverTaskAdded(), the raw pointer of |task| is
      // remebered somewhere else. One should not (implicitly) delete the
      // referenced object before calling NotifyObserverTaskRemoved() first
      // (crbug.com/587707).
      DCHECK(!task.get()) <<
          "Task with the same pid should not be added twice.";
      task = std::make_unique<ArcProcessTask>(std::move(entry));
      NotifyObserverTaskAdded(task.get());
    } else {
      // Update process state of existing process.
      std::unique_ptr<ArcProcessTask>& task = (*pid_to_task)[entry.nspid()];
      DCHECK(task.get());
      task->SetProcessState(entry.process_state());
    }
  }

  for (const auto& entry : nspid_to_remove) {
    // Stale arc process.
    NotifyObserverTaskRemoved((*pid_to_task)[entry].get());
    pid_to_task->erase(entry);
  }
}

void ArcProcessTaskProvider::OnUpdateAppProcessList(
    OptionalArcProcessList processes) {
  if (!processes) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextAppRequest();
    return;
  }

  TRACE_EVENT0("browser", "ArcProcessTaskProvider::OnUpdateAppProcessList");
  UpdateProcessList(&nspid_to_task_, std::move(*processes));
  ScheduleNextAppRequest();
}

void ArcProcessTaskProvider::OnUpdateSystemProcessList(
    OptionalArcProcessList processes) {
  if (processes)
    UpdateProcessList(&nspid_to_sys_task_, std::move(*processes));
  ScheduleNextSystemRequest();
}

void ArcProcessTaskProvider::RequestAppProcessList() {
  arc::ArcProcessService* arc_process_service =
      arc::ArcProcessService::Get();
  if (!arc_process_service) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextAppRequest();
    return;
  }

  auto callback =
      base::BindOnce(&ArcProcessTaskProvider::OnUpdateAppProcessList,
                     weak_ptr_factory_.GetWeakPtr());
  arc_process_service->RequestAppProcessList(std::move(callback));
}

void ArcProcessTaskProvider::RequestSystemProcessList() {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextSystemRequest();
    return;
  }

  auto callback =
      base::BindOnce(&ArcProcessTaskProvider::OnUpdateSystemProcessList,
                     weak_ptr_factory_.GetWeakPtr());
  arc_process_service->RequestSystemProcessList(std::move(callback));
}

void ArcProcessTaskProvider::StartUpdating() {
  is_updating_ = true;
  RequestAppProcessList();
  RequestSystemProcessList();
}

void ArcProcessTaskProvider::StopUpdating() {
  is_updating_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  nspid_to_task_.clear();
  nspid_to_sys_task_.clear();
}

void ArcProcessTaskProvider::ScheduleNextRequest(base::OnceClosure task) {
  if (!is_updating_)
    return;
  // TODO(nya): Remove this timer once ARC starts to send us UpdateProcessList
  // message when the process list changed. As of today, ARC does not send
  // the process list unless we request it by RequestAppProcessList message.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(task),
      arc::ArcProcessService::kProcessSnapshotRefreshTime);
}

void ArcProcessTaskProvider::ScheduleNextAppRequest() {
  ScheduleNextRequest(
      base::BindOnce(&ArcProcessTaskProvider::RequestAppProcessList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcProcessTaskProvider::ScheduleNextSystemRequest() {
  ScheduleNextRequest(
      base::BindOnce(&ArcProcessTaskProvider::RequestSystemProcessList,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace task_manager
