// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/crosapi_task_provider_ash.h"

#include <set>
#include <vector>

#include "base/check_op.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/task_manager/providers/crosapi/crosapi_task.h"
#include "chrome/browser/task_manager/sampling/task_group.h"

namespace task_manager {

CrosapiTaskProviderAsh::CrosapiTaskProviderAsh() {
  // CrosapiManager is not initialized on unit testing.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->task_manager_ash()
        ->SetObserver(this);
  }
}

CrosapiTaskProviderAsh::~CrosapiTaskProviderAsh() {
  // When user signs out from ash, CrospaiManager instance owned by
  // `ChromeBrowserMainPartsAsh` is destroyed before `TaskManagerImpl`
  // instance, which is a lazy instance. We should always make sure
  // CrosapiManager::IsInitialized  before accessing it.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->task_manager_ash()
        ->RemoveObserver();
  }
}

Task* CrosapiTaskProviderAsh::GetTaskOfUrlRequest(int child_id, int route_id) {
  return nullptr;
}

void CrosapiTaskProviderAsh::RefreshTaskGroup(TaskGroup* task_group) {
  auto it = pid_to_task_groups_.find(task_group->process_id());
  if (it == pid_to_task_groups_.end()) {
    LOG(WARNING) << "Can't find task group for pid:"
                 << task_group->process_id();
    return;
  }

  crosapi::mojom::TaskGroupPtr& cached_task_group = it->second;
  task_group->set_platform_independent_cpu_usage(
      cached_task_group->platform_independent_cpu_usage);
  task_group->set_footprint_bytes(cached_task_group->memory_footprint_bytes);
  task_group->set_swapped_bytes(cached_task_group->swapped_mem_bytes);
  task_group->set_gpu_memory(cached_task_group->gpu_memory_bytes);
  task_group->set_gpu_memory_has_duplicates(
      cached_task_group->gpu_memory_has_duplicates);
  task_group->set_is_backgrounded(cached_task_group->is_backgrounded);
#if BUILDFLAG(ENABLE_NACL)
  task_group->set_nacl_debug_stub_port(cached_task_group->nacl_debug_stub_port);
#endif  // BUILDFLAG(ENABLE_NACL)
  task_group->set_open_fd_count(cached_task_group->open_fd_count);
  task_group->set_idle_wakeups_per_second(
      cached_task_group->idle_wakeups_per_second);
}

const TaskIdList& CrosapiTaskProviderAsh::GetSortedTaskIds() {
  return sorted_task_ids_;
}

void CrosapiTaskProviderAsh::StartUpdating() {
  // CrosapiManager is not initialized on unit testing and do not start
  // refresh_timer_ on unit testing.
  if (!crosapi::CrosapiManager::IsInitialized())
    return;

  refresh_timer_.Start(
      FROM_HERE, base::Seconds(1),
      base::BindRepeating(&CrosapiTaskProviderAsh::GetCrosapiTaskManagerTasks,
                          base::Unretained(this)));
}

void CrosapiTaskProviderAsh::StopUpdating() {
  // Task manager has stopped updating (Task manager UI closed). Clean up cached
  // data and notify lacros about the event.
  refresh_timer_.Stop();
  CleanupCachedData();

  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->task_manager_ash()
        ->OnTaskManagerClosed();
  }
}

void CrosapiTaskProviderAsh::OnTaskManagerProviderDisconnected() {
  // No more task manager clients (lacros has been shut down), remove all
  // lacros tasks and clean up.
  for (const auto& item : uuid_to_task_)
    NotifyObserverTaskRemoved(item.second.get());

  CleanupCachedData();
}

void CrosapiTaskProviderAsh::GetCrosapiTaskManagerTasks() {
  if (!crosapi::CrosapiManager::IsInitialized())
    return;

  // Get lacros tasks if there is any task manager provider registered and ash
  // task manager has been set up to refresh with a valid refresh interval.
  auto* task_manager_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->task_manager_ash();
  if (task_manager_ash->HasRegisteredProviders()) {
    task_manager_ash->GetTaskManagerTasks(
        base::BindOnce(&CrosapiTaskProviderAsh::OnGetTaskManagerTasks,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CrosapiTaskProviderAsh::OnGetTaskManagerTasks(
    std::vector<crosapi::mojom::TaskPtr> task_results,
    std::vector<crosapi::mojom::TaskGroupPtr> task_group_results,
    const std::optional<std::string>& active_task_uuid) {
  // Ignore the data returned from the previous crosapi GetTaskManagrTasks
  // call which is issued before StopUpdating().
  if (!IsUpdating())
    return;

  DCHECK_EQ(uuid_to_task_.size(), sorted_task_ids_.size());

  std::set<std::string> uuid_to_remove;
  for (const auto& item : uuid_to_task_)
    uuid_to_remove.insert(item.first);

  bool task_added_or_removed = false;

  for (const auto& mojo_task : task_results) {
    uuid_to_remove.erase(mojo_task->task_uuid);
  }

  // Remove the stale lacros tasks. Doing this before adding or updating task
  // can reduce chance needing to move existing tasks.
  for (const auto& uuid : uuid_to_remove) {
    NotifyObserverTaskRemoved(uuid_to_task_[uuid].get());
    std::erase(sorted_task_ids_, uuid_to_task_[uuid]->task_id());
    uuid_to_task_.erase(uuid);
    DCHECK_EQ(uuid_to_task_.size(), sorted_task_ids_.size());
    task_added_or_removed = true;
  }

  std::optional<TaskId> last_updated_task_id;
  for (const auto& mojo_task : task_results) {
    const bool new_task = !base::Contains(uuid_to_task_, mojo_task->task_uuid);
    std::unique_ptr<CrosapiTask>& task = uuid_to_task_[mojo_task->task_uuid];
    // Find the next position of |last_updated_task_id| to add or update the
    // current task.
    std::vector<TaskId>::iterator destination_it = sorted_task_ids_.begin();
    if (last_updated_task_id.has_value()) {
      auto it = std::find(sorted_task_ids_.begin(), sorted_task_ids_.end(),
                          last_updated_task_id.value());
      if (it != sorted_task_ids_.end()) {
        destination_it = it + 1;
      }
    }
    if (new_task) {
      // New lacros task.
      task_added_or_removed = true;
      DCHECK(!task.get());
      task = std::make_unique<CrosapiTask>(mojo_task);
      // Since NotifyObserverTaskAdded will cause task manager to rebuild its
      // GetSortedTaskIds() with |sorted_task_ids_|, and also trigger the UI to
      // update with new task added. We must make sure |sorted_task_ids_|
      // always contains all the crosapi tasks before calling
      // NotifyObserverTaskAdded.
      sorted_task_ids_.insert(destination_it, task->task_id());
      DCHECK_EQ(uuid_to_task_.size(), sorted_task_ids_.size());
      NotifyObserverTaskAdded(task.get());
    } else {
      // Update existing lacros task.
      // The existing task does not guarantee to have the same order so we need
      // to reinsert to the right position if needed. The following will move
      // the existing task from origin_it to destination_it if they are
      // different. Task Manager does not support move operation currently,
      // which may make selection incorrect when it happens. But since reorder
      // of existing tasks can rarely happen it's acceptable to leave it as is
      // for now.
      // TODO(b/329101660): Report move operation to |TableModelObserver|.
      std::vector<TaskId>::iterator origin_it = std::find(
          sorted_task_ids_.begin(), sorted_task_ids_.end(), task->task_id());
      // Insert or remove from the back the vector first.
      if (origin_it < destination_it) {
        sorted_task_ids_.insert(destination_it, task->task_id());
        sorted_task_ids_.erase(origin_it);
      } else if (origin_it > destination_it) {
        sorted_task_ids_.erase(origin_it);
        sorted_task_ids_.insert(destination_it, task->task_id());
      }
      task->Update(mojo_task);
      if (task->process_id() != mojo_task->process_id) {
        UpdateTaskProcessInfoAndNotifyObserver(
            task.get(), mojo_task->process_id, mojo_task->process_id);
      }
    }
    last_updated_task_id = task->task_id();
  }

  DCHECK_EQ(task_results.size(), uuid_to_task_.size());

  // Cache task group data.
  pid_to_task_groups_.clear();
  for (auto& mojo_task_group : task_group_results) {
    pid_to_task_groups_[mojo_task_group->process_id] =
        std::move(mojo_task_group);
  }

  // Notify task manager to invalidate GetTaskIdsList if there are any lacros
  // tasks added or removed, since adding or removing tasks will cause
  // lacros tasks to be re-sorted in lacros. Ash task manager UI won't get the
  // complete list of the new sorted lacros task ids until task manager
  // invalidate and rebuild the sorted task list in GetTaskIdsList().
  DCHECK_EQ(task_results.size(), sorted_task_ids_.size());
  if (task_added_or_removed)
    NotifyObserverTaskIdsListToBeInvalidated();

  if (!active_task_uuid.has_value()) {
    return;
  }
  const auto it = uuid_to_task_.find(active_task_uuid.value());
  if (it != uuid_to_task_.end()) {
    NotifyObserverActiveTaskFetched(it->second->task_id());
  }
}

void CrosapiTaskProviderAsh::CleanupCachedData() {
  uuid_to_task_.clear();
  sorted_task_ids_.clear();
  pid_to_task_groups_.clear();
}

}  // namespace task_manager
