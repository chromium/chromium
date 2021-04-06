// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/task_manager_controller_lacros.h"

#include "base/guid.h"
#include "base/time/time.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
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
    : TaskManagerObserver(base::TimeDelta::FromSeconds(1), REFRESH_TYPE_NONE) {}

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

  std::vector<crosapi::mojom::TaskPtr> task_results;
  std::vector<crosapi::mojom::TaskGroupPtr> task_group_results;
  for (auto& item : id_to_tasks_)
    task_results.push_back(item.second.Clone());

  // TODO(crbug.com/1148572): Implement getting |task_group_results|.

  std::move(callback).Run(std::move(task_results),
                          std::move(task_group_results));
}

void TaskManagerControllerLacros::OnTaskManagerClosed() {
  // Task manager closed in ash, clean up cached task data and stop observing
  // lacros task manager.
  id_to_tasks_.clear();

  if (observed_task_manager())
    observed_task_manager()->RemoveObserver(this);
}

void TaskManagerControllerLacros::OnTaskAdded(TaskId id) {
  id_to_tasks_[id] = ToMojoTask(id);
}

void TaskManagerControllerLacros::OnTaskToBeRemoved(TaskId id) {
  id_to_tasks_.erase(id);
}

void TaskManagerControllerLacros::OnTasksRefreshed(const TaskIdList& task_ids) {
  // Update tasks.
  for (auto& item : id_to_tasks_) {
    TaskId id = item.first;
    crosapi::mojom::TaskPtr& mojo_task = item.second;
    UpdateTask(id, mojo_task);
  }
}

crosapi::mojom::TaskPtr TaskManagerControllerLacros::ToMojoTask(TaskId id) {
  auto mojo_task = crosapi::mojom::Task::New();
  mojo_task->task_uuid = base::GenerateGUID();
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

}  // namespace task_manager
