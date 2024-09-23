// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/task_manager_controller_lacros.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "third_party/blink/public/common/web_cache/web_cache_resource_type_stats.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

namespace {

crosapi::mojom::TaskType ToMojo(task_manager::Task::Type type) {
  switch (type) {
    case task_manager::Task::BROWSER:
      return crosapi::mojom::TaskType::kBrowser;
    case task_manager::Task::GPU:
      return crosapi::mojom::TaskType::kGpu;
    case task_manager::Task::ZYGOTE:
      return crosapi::mojom::TaskType::kZygote;
    case task_manager::Task::UTILITY:
      return crosapi::mojom::TaskType::kUtility;
    case task_manager::Task::RENDERER:
      return crosapi::mojom::TaskType::kRenderer;
    case task_manager::Task::EXTENSION:
      return crosapi::mojom::TaskType::kExtension;
    case task_manager::Task::GUEST:
      return crosapi::mojom::TaskType::kGuest;
    case task_manager::Task::PLUGIN:
      return crosapi::mojom::TaskType::kPlugin;
    case task_manager::Task::NACL:
      return crosapi::mojom::TaskType::kNacl;
    case task_manager::Task::SANDBOX_HELPER:
      return crosapi::mojom::TaskType::kSandboxHelper;
    case task_manager::Task::DEDICATED_WORKER:
      return crosapi::mojom::TaskType::kDedicatedWorker;
    case task_manager::Task::SHARED_WORKER:
      return crosapi::mojom::TaskType::kSharedWorker;
    case task_manager::Task::SERVICE_WORKER:
      return crosapi::mojom::TaskType::kServiceWorker;
    default:
      return crosapi::mojom::TaskType::kUnknown;
  }
}

crosapi::mojom::WebCacheResourceTypeStatPtr ToMojo(
    const blink::WebCacheResourceTypeStat& stat) {
  return crosapi::mojom::WebCacheResourceTypeStat::New(stat.count, stat.size,
                                                       stat.decoded_size);
}

crosapi::mojom::WebCacheResourceTypeStatsPtr ToMojo(
    const blink::WebCacheResourceTypeStats& stats) {
  return crosapi::mojom::WebCacheResourceTypeStats::New(
      ToMojo(stats.images), ToMojo(stats.css_style_sheets),
      ToMojo(stats.scripts), ToMojo(stats.xsl_style_sheets),
      ToMojo(stats.fonts), ToMojo(stats.other));
}

}  // namespace

TaskManagerControllerLacros::TaskManagerControllerLacros()
    : TaskManagerObserver(base::Seconds(1), REFRESH_TYPE_NONE) {}

TaskManagerControllerLacros::~TaskManagerControllerLacros() {
  if (observed_task_manager())
    TaskManagerInterface::GetTaskManager()->RemoveObserver(this);
}

void TaskManagerControllerLacros::SetRefreshFlags(int64_t refresh_flags) {
  // Update the refresh flags and notify its observed task manager.
  if (refresh_flags != desired_resources_flags())
    SetRefreshTypesFlags(refresh_flags);

  // Add the TaskManagerControllerLacros to observe the lacros task manager if
  // |refresh_flags| is valid, which indicates ash task manager has started
  // updating.
  if (!observed_task_manager() && refresh_flags != REFRESH_TYPE_NONE)
    TaskManagerInterface::GetTaskManager()->AddObserver(this);
}

void TaskManagerControllerLacros::GetTaskManagerTasks(
    GetTaskManagerTasksCallback callback) {
  DCHECK(observed_task_manager());

  std::optional<TaskId> active_task_id;
  std::string active_task_uuid;
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    if (content::WebContents* active_web_contents =
            browser->tab_strip_model()->GetActiveWebContents()) {
      active_task_id =
          observed_task_manager()->GetTaskIdForWebContents(active_web_contents);
    }
  }

  std::set<TaskId> task_ids_to_remove;
  for (const auto& item : id_to_tasks_)
    task_ids_to_remove.insert(item.first);

  // Get the sorted list of the task IDs from Lacros task manager.
  // Place Lacros tasks in the same order when sending to ash.
  std::vector<crosapi::mojom::TaskPtr> task_results;
  for (const auto& task_id : observed_task_manager()->GetTaskIdsList()) {
    if (task_ids_to_remove.erase(task_id) == 0) {
      // New task.
      id_to_tasks_[task_id] = ToMojoTask(task_id);
    } else {
      // Update existing task.
      crosapi::mojom::TaskPtr& mojo_task = id_to_tasks_[task_id];
      UpdateTask(task_id, mojo_task);
    }
    task_results.push_back(id_to_tasks_[task_id].Clone());

    if (task_id == active_task_id) {
      active_task_uuid = id_to_tasks_[task_id]->task_uuid;
    }
  }

  // Remove stale tasks.
  for (const auto& task_id : task_ids_to_remove)
    id_to_tasks_.erase(task_id);

  // Retrieve and return the task groups.
  std::set<base::ProcessId> pids;
  std::vector<crosapi::mojom::TaskGroupPtr> task_group_results;
  for (const auto& item : id_to_tasks_) {
    const TaskId task_id = item.first;
    const base::ProcessId pid = observed_task_manager()->GetProcessId(task_id);
    auto result = pids.insert(pid);
    if (result.second) {
      // New task group.
      task_group_results.push_back(ToMojoTaskGroup(pid, task_id));
    }
  }

  std::move(callback).Run(std::move(task_results),
                          std::move(task_group_results), active_task_uuid);
}

void TaskManagerControllerLacros::OnTaskManagerClosed() {
  // Task manager closed in ash, clean up cached task data and stop observing
  // lacros task manager.
  id_to_tasks_.clear();

  if (observed_task_manager())
    observed_task_manager()->RemoveObserver(this);
}

void TaskManagerControllerLacros::ActivateTask(const std::string& task_uuid) {
  for (const auto& item : id_to_tasks_) {
    if (item.second->task_uuid == task_uuid) {
      TaskId task_id = item.first;
      // Check if the task is still valid.
      // Note: It is very rare but possible that lacros receives the request
      // from ash to remove a task after the task has been removed from lacros
      // task manager but before the cached |id_to_tasks_| is refreshed in
      // GetTaskManagerTasks() call.
      if (base::Contains(observed_task_manager()->GetTaskIdsList(), task_id)) {
        observed_task_manager()->ActivateTask(task_id);
      }
      return;
    }
  }
}

crosapi::mojom::TaskPtr TaskManagerControllerLacros::ToMojoTask(TaskId id) {
  auto mojo_task = crosapi::mojom::Task::New();
  mojo_task->task_uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  mojo_task->type = ToMojo(observed_task_manager()->GetType(id));
  UpdateTask(id, mojo_task);
  return mojo_task;
}

void TaskManagerControllerLacros::UpdateTask(
    TaskId id,
    crosapi::mojom::TaskPtr& mojo_task) {
  mojo_task->title = observed_task_manager()->GetTitle(id);
  mojo_task->process_id = observed_task_manager()->GetProcessId(id);
  gfx::ImageSkia icon = observed_task_manager()->GetIcon(id);
  mojo_task->icon = icon.DeepCopy();
  mojo_task->profile_name = observed_task_manager()->GetProfileName(id);
  mojo_task->used_sqlite_memory =
      observed_task_manager()->GetSqliteMemoryUsed(id);

  int64_t memory_allocated, memory_used;
  if (observed_task_manager()->GetV8Memory(id, &memory_allocated,
                                           &memory_used)) {
    mojo_task->v8_memory_allocated = memory_allocated;
    mojo_task->v8_memory_used = memory_used;
  } else {
    mojo_task->v8_memory_allocated = -1;
    mojo_task->v8_memory_used = -1;
  }

  blink::WebCacheResourceTypeStats stats;
  if (observed_task_manager()->GetWebCacheStats(id, &stats))
    mojo_task->web_cache_stats = ToMojo(stats);

  mojo_task->keep_alive_count = observed_task_manager()->GetKeepaliveCount(id);
  mojo_task->network_usage_rate = observed_task_manager()->GetNetworkUsage(id);
  mojo_task->cumulative_network_usage =
      observed_task_manager()->GetCumulativeNetworkUsage(id);
}

crosapi::mojom::TaskGroupPtr TaskManagerControllerLacros::ToMojoTaskGroup(
    base::ProcessId pid,
    TaskId task_id) {
  auto mojo_task_group = crosapi::mojom::TaskGroup::New();
  mojo_task_group->process_id = pid;
  UpdateTaskGroup(pid, task_id, mojo_task_group);
  return mojo_task_group;
}

void TaskManagerControllerLacros::UpdateTaskGroup(
    base::ProcessId pid,
    TaskId task_id,
    crosapi::mojom::TaskGroupPtr& mojo_task_group) {
  mojo_task_group->platform_independent_cpu_usage =
      observed_task_manager()->GetPlatformIndependentCPUUsage(task_id);
  mojo_task_group->memory_footprint_bytes =
      observed_task_manager()->GetMemoryFootprintUsage(task_id);
  mojo_task_group->swapped_mem_bytes =
      observed_task_manager()->GetSwappedMemoryUsage(task_id);
  mojo_task_group->gpu_memory_bytes =
      observed_task_manager()->GetGpuMemoryUsage(
          task_id, &mojo_task_group->gpu_memory_has_duplicates);
  mojo_task_group->is_backgrounded =
      observed_task_manager()->IsTaskOnBackgroundedProcess(task_id);
  mojo_task_group->nacl_debug_stub_port =
      observed_task_manager()->GetNaClDebugStubPort(task_id);
  mojo_task_group->open_fd_count =
      observed_task_manager()->GetOpenFdCount(task_id);
  mojo_task_group->idle_wakeups_per_second =
      observed_task_manager()->GetIdleWakeupsPerSecond(task_id);
}

}  // namespace task_manager
