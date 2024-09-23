// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/task_manager/providers/browser_process_task_provider.h"
#include "chrome/browser/task_manager/providers/child_process_task_provider.h"
#include "chrome/browser/task_manager/providers/fallback_task_provider.h"
#include "chrome/browser/task_manager/providers/render_process_host_task_provider.h"
#include "chrome/browser/task_manager/providers/spare_render_process_host_task_provider.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "chrome/browser/task_manager/providers/worker_task_provider.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/task_manager/providers/arc/arc_process_task_provider.h"
#include "chrome/browser/task_manager/providers/crosapi/crosapi_task_provider_ash.h"
#include "chrome/browser/task_manager/providers/vm/vm_process_task_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace task_manager {

namespace {

base::LazyInstance<TaskManagerImpl>::Leaky lazy_task_manager_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TaskManagerImpl::TaskManagerImpl()
    : on_background_data_ready_callback_(base::BindRepeating(
          &TaskManagerImpl::OnTaskGroupBackgroundCalculationsDone,
          base::Unretained(this))),
      blocking_pool_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      shared_sampler_(new SharedSampler(blocking_pool_runner_)),
      is_running_(false),
      waiting_for_memory_dump_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_providers_.push_back(std::make_unique<BrowserProcessTaskProvider>());
  task_providers_.push_back(std::make_unique<ChildProcessTaskProvider>());

  // Put all task providers for various types of RenderProcessHosts in this
  // section. All of them should be added as primary subproviders for the
  // FallbackTaskProvider, so that a fallback task can be shown for a renderer
  // process if no other provider is shown for it.
  std::vector<std::unique_ptr<TaskProvider>> primary_subproviders;
  primary_subproviders.push_back(
      std::make_unique<SpareRenderProcessHostTaskProvider>());
  primary_subproviders.push_back(std::make_unique<WorkerTaskProvider>());
  primary_subproviders.push_back(std::make_unique<WebContentsTaskProvider>());
  task_providers_.push_back(std::make_unique<FallbackTaskProvider>(
      std::move(primary_subproviders),
      std::make_unique<RenderProcessHostTaskProvider>()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (arc::IsArcAvailable())
    task_providers_.push_back(std::make_unique<ArcProcessTaskProvider>());
  task_providers_.push_back(std::make_unique<VmProcessTaskProvider>());
  arc_shared_sampler_ = std::make_unique<ArcSharedSampler>();

  if (crosapi::browser_util::IsLacrosEnabled()) {
    std::unique_ptr<CrosapiTaskProviderAsh> task_provider =
        std::make_unique<CrosapiTaskProviderAsh>();
    crosapi_task_provider_ = task_provider.get();
    task_providers_.push_back(std::move(task_provider));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TaskManagerImpl::~TaskManagerImpl() {
  StopUpdating();
}

// static
TaskManagerImpl* TaskManagerImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return lazy_task_manager_instance.Pointer();
}

bool TaskManagerImpl::IsCreated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return lazy_task_manager_instance.IsCreated();
}

void TaskManagerImpl::ActivateTask(TaskId task_id) {
  GetTaskByTaskId(task_id)->Activate();
}

bool TaskManagerImpl::IsTaskKillable(TaskId task_id) {
  return GetTaskByTaskId(task_id)->IsKillable();
}

void TaskManagerImpl::KillTask(TaskId task_id) {
  GetTaskByTaskId(task_id)->Kill();
}

double TaskManagerImpl::GetPlatformIndependentCPUUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->platform_independent_cpu_usage();
}

base::Time TaskManagerImpl::GetStartTime(TaskId task_id) const {
#if BUILDFLAG(IS_WIN)
  return GetTaskGroupByTaskId(task_id)->start_time();
#else
  NOTIMPLEMENTED();
  return base::Time();
#endif
}

base::TimeDelta TaskManagerImpl::GetCpuTime(TaskId task_id) const {
#if BUILDFLAG(IS_WIN)
  return GetTaskGroupByTaskId(task_id)->cpu_time();
#else
  NOTIMPLEMENTED();
  return base::TimeDelta();
#endif
}

int64_t TaskManagerImpl::GetMemoryFootprintUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->footprint_bytes();
}

int64_t TaskManagerImpl::GetSwappedMemoryUsage(TaskId task_id) const {
#if BUILDFLAG(IS_CHROMEOS)
  return GetTaskGroupByTaskId(task_id)->swapped_bytes();
#else
  return -1;
#endif
}

int64_t TaskManagerImpl::GetGpuMemoryUsage(TaskId task_id,
                                           bool* has_duplicates) const {
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  if (has_duplicates)
    *has_duplicates = task_group->gpu_memory_has_duplicates();
  return task_group->gpu_memory();
}

int TaskManagerImpl::GetIdleWakeupsPerSecond(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->idle_wakeups_per_second();
}

int TaskManagerImpl::GetHardFaultsPerSecond(TaskId task_id) const {
#if BUILDFLAG(IS_WIN)
  return GetTaskGroupByTaskId(task_id)->hard_faults_per_second();
#else
  return -1;
#endif
}

int TaskManagerImpl::GetNaClDebugStubPort(TaskId task_id) const {
#if BUILDFLAG(ENABLE_NACL)
  return GetTaskGroupByTaskId(task_id)->nacl_debug_stub_port();
#else
  return -2;
#endif  // BUILDFLAG(ENABLE_NACL)
}

void TaskManagerImpl::GetGDIHandles(TaskId task_id,
                                    int64_t* current,
                                    int64_t* peak) const {
#if BUILDFLAG(IS_WIN)
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  *current = task_group->gdi_current_handles();
  *peak = task_group->gdi_peak_handles();
#else
  *current = -1;
  *peak = -1;
#endif  // BUILDFLAG(IS_WIN)
}

void TaskManagerImpl::GetUSERHandles(TaskId task_id,
                                     int64_t* current,
                                     int64_t* peak) const {
#if BUILDFLAG(IS_WIN)
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  *current = task_group->user_current_handles();
  *peak = task_group->user_peak_handles();
#else
  *current = -1;
  *peak = -1;
#endif  // BUILDFLAG(IS_WIN)
}

int TaskManagerImpl::GetOpenFdCount(TaskId task_id) const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return GetTaskGroupByTaskId(task_id)->open_fd_count();
#else
  return -1;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
}

bool TaskManagerImpl::IsTaskOnBackgroundedProcess(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->is_backgrounded();
}

const std::u16string& TaskManagerImpl::GetTitle(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->title();
}

std::u16string TaskManagerImpl::GetProfileName(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetProfileName();
}

const gfx::ImageSkia& TaskManagerImpl::GetIcon(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->icon();
}

const base::ProcessHandle& TaskManagerImpl::GetProcessHandle(
    TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->process_handle();
}

const base::ProcessId& TaskManagerImpl::GetProcessId(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->process_id();
}

Task::Type TaskManagerImpl::GetType(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetType();
}

SessionID TaskManagerImpl::GetTabId(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetTabId();
}

int TaskManagerImpl::GetChildProcessUniqueId(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetChildProcessUniqueID();
}

void TaskManagerImpl::GetTerminationStatus(TaskId task_id,
                                           base::TerminationStatus* out_status,
                                           int* out_error_code) const {
  GetTaskByTaskId(task_id)->GetTerminationStatus(out_status, out_error_code);
}

int64_t TaskManagerImpl::GetNetworkUsage(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetNetworkUsageRate();
}

int64_t TaskManagerImpl::GetCumulativeNetworkUsage(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetCumulativeNetworkUsage();
}

int64_t TaskManagerImpl::GetProcessTotalNetworkUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->per_process_network_usage_rate();
}

int64_t TaskManagerImpl::GetCumulativeProcessTotalNetworkUsage(
    TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->cumulative_per_process_network_usage();
}

int64_t TaskManagerImpl::GetSqliteMemoryUsed(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetSqliteMemoryUsed();
}

bool TaskManagerImpl::GetV8Memory(TaskId task_id,
                                  int64_t* allocated,
                                  int64_t* used) const {
  const Task* task = GetTaskByTaskId(task_id);
  const int64_t allocated_memory = task->GetV8MemoryAllocated();
  const int64_t used_memory = task->GetV8MemoryUsed();
  if (allocated_memory == -1 || used_memory == -1)
    return false;

  *allocated = allocated_memory;
  *used = used_memory;

  return true;
}

bool TaskManagerImpl::GetWebCacheStats(
    TaskId task_id,
    blink::WebCacheResourceTypeStats* stats) const {
  const Task* task = GetTaskByTaskId(task_id);
  if (!task->ReportsWebCacheStats())
    return false;

  *stats = task->GetWebCacheStats();

  return true;
}

int TaskManagerImpl::GetKeepaliveCount(TaskId task_id) const {
  const Task* task = GetTaskByTaskId(task_id);
  if (!task)
    return -1;

  return task->GetKeepaliveCount();
}

const TaskIdList& TaskManagerImpl::GetTaskIdsList() const {
  DCHECK(is_running_) << "Task manager is not running. You must observe the "
                         "task manager for it to start running";

  if (sorted_task_ids_.empty()) {
    // |comparator| groups and sorts by subtask-ness (to push all subtasks to be
    // last), then by process type (e.g. the browser process should be first;
    // renderer processes should be together), then tab id (processes used by
    // the same tab should be kept together, and a tab should have a stable
    // position in the list as it cycles through processes, and tab creation
    // order is meaningful), and finally by task id (when all else is equal, put
    // the oldest tasks first).
    auto comparator = [](const Task* a, const Task* b) -> bool {
      return std::make_tuple(a->HasParentTask(), a->GetType(), a->GetTabId(),
                             a->task_id()) <
             std::make_tuple(b->HasParentTask(), b->GetType(), b->GetTabId(),
                             b->task_id());
    };

    const size_t num_groups =
        task_groups_by_proc_id_.size() + arc_vm_task_groups_by_proc_id_.size();

    // |task_groups_by_task_id_| contains all tasks, both VM and non-VM.
    const size_t num_tasks = task_groups_by_task_id_.size();

    // Populate |tasks_to_visit| with one task from each group.
    std::vector<const Task*> tasks_to_visit;
    tasks_to_visit.reserve(num_groups);
    std::unordered_map<const Task*, std::vector<const Task*>> children;
    for (const auto& groups_pair : task_groups_by_proc_id_) {
      // The first task in the group (per |comparator|) is the one used for
      // sorting the group relative to other groups.
      const std::vector<raw_ptr<Task, VectorExperimental>>& tasks =
          groups_pair.second->tasks();
      Task* group_task =
          *std::min_element(tasks.begin(), tasks.end(), comparator);
      tasks_to_visit.push_back(group_task);

      // Build the parent-to-child map, for use later.
      for (const Task* task : tasks) {
        if (task->HasParentTask())
          children[task->GetParentTask()].push_back(task);
        else
          DCHECK(!group_task->HasParentTask());
      }
    }

    for (const auto& groups_pair : arc_vm_task_groups_by_proc_id_) {
      const std::vector<raw_ptr<Task, VectorExperimental>>& tasks =
          groups_pair.second->tasks();
      Task* group_task =
          *std::min_element(tasks.begin(), tasks.end(), comparator);
      tasks_to_visit.push_back(group_task);
    }

    // Now sort |tasks_to_visit| in reverse order (putting the browser process
    // at back()). We will treat it as a stack from now on.
    std::sort(tasks_to_visit.rbegin(), tasks_to_visit.rend(), comparator);
    DCHECK_EQ(Task::BROWSER, tasks_to_visit.back()->GetType());

    // Using |tasks_to_visit| as a stack, and |visited_groups| to track which
    // groups we've already added, add groups to |sorted_task_ids_| until all
    // groups have been added.
    sorted_task_ids_.reserve(num_tasks);
    std::unordered_set<TaskGroup*> visited_groups;
    visited_groups.reserve(num_groups);
    std::vector<raw_ptr<Task, VectorExperimental>>
        current_group_tasks;  // Outside loop for fewer mallocs.
    while (visited_groups.size() < num_groups) {
      DCHECK(!tasks_to_visit.empty());
      TaskGroup* current_group =
          GetTaskGroupByTaskId(tasks_to_visit.back()->task_id());
      tasks_to_visit.pop_back();

      // Mark |current_group| as visited. If this fails, we've already added
      // the group, and should skip over it.
      if (!visited_groups.insert(current_group).second)
        continue;

      // Make a copy of |current_group->tasks()|, sort it, and append the ids.
      current_group_tasks = current_group->tasks();
      std::sort(current_group_tasks.begin(), current_group_tasks.end(),
                comparator);
      for (Task* task : current_group_tasks)
        sorted_task_ids_.push_back(task->task_id());

      // Find the children of the tasks we just added, and push them into
      // |tasks_to_visit|, so that we visit them soon. Work in reverse order,
      // so that we visit them in forward order.
      for (Task* parent : base::Reversed(current_group_tasks)) {
        auto children_of_parent = children.find(parent);
        if (children_of_parent != children.end()) {
          // Sort children[parent], and then append in reversed order.
          std::sort(children_of_parent->second.begin(),
                    children_of_parent->second.end(), comparator);
          tasks_to_visit.insert(tasks_to_visit.end(),
                                children_of_parent->second.rbegin(),
                                children_of_parent->second.rend());
        }
      }
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (crosapi_task_provider_) {
      // Lacros tasks have been pre-sorted in lacros. Place them at the
      // end of |sorted_task_ids| in the same order they are returned from
      // crosapi.
      for (const auto& task_id : crosapi_task_provider_->GetSortedTaskIds()) {
        Task* task = GetTaskByTaskId(task_id);
        if (task)
          sorted_task_ids_.push_back(task_id);
      }
    }
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

    DCHECK_EQ(num_tasks, sorted_task_ids_.size());
  }

  return sorted_task_ids_;
}

TaskIdList TaskManagerImpl::GetIdsOfTasksSharingSameProcess(
    TaskId task_id) const {
  DCHECK(is_running_) << "Task manager is not running. You must observe the "
                         "task manager for it to start running";

  TaskIdList result;
  TaskGroup* group = GetTaskGroupByTaskId(task_id);
  if (group) {
    result.reserve(group->tasks().size());
    for (Task* task : group->tasks())
      result.push_back(task->task_id());
  }
  return result;
}

size_t TaskManagerImpl::GetNumberOfTasksOnSameProcess(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->num_tasks();
}

bool TaskManagerImpl::IsRunningInVM(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->IsRunningInVM();
}

TaskId TaskManagerImpl::GetTaskIdForWebContents(
    content::WebContents* web_contents) const {
  if (!web_contents)
    return -1;
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  Task* task = GetTaskByRoute(rfh->GetGlobalId());
  if (!task)
    return -1;
  return task->task_id();
}

void TaskManagerImpl::TaskAdded(Task* task) {
  DCHECK(task);

  const base::ProcessId proc_id = task->process_id();
  const TaskId task_id = task->task_id();
  const bool is_running_in_vm = task->IsRunningInVM();
  const bool is_running_in_lacros = task->GetType() == Task::LACROS;
  TaskManagerImpl::PidToTaskGroupMap& task_group_map =
      is_running_in_vm ? arc_vm_task_groups_by_proc_id_
                       : is_running_in_lacros ? crosapi_task_groups_by_proc_id_
                                              : task_groups_by_proc_id_;

  std::unique_ptr<TaskGroup>& task_group = task_group_map[proc_id];
  if (!task_group) {
    task_group = std::make_unique<TaskGroup>(
        task->process_handle(), proc_id, is_running_in_vm,
        on_background_data_ready_callback_, shared_sampler_,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        is_running_in_lacros ? crosapi_task_provider_.get() : nullptr,
#endif
        blocking_pool_runner_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (task->GetType() == Task::ARC)
      task_group->SetArcSampler(arc_shared_sampler_.get());
#endif
  }

  task_group->AddTask(task);

  task_groups_by_task_id_[task_id] = task_group.get();

  // Invalidate the cached sorted IDs by clearing the list.
  sorted_task_ids_.clear();

  NotifyObserversOnTaskAdded(task_id);
}

void TaskManagerImpl::TaskRemoved(Task* task) {
  DCHECK(task);

  const base::ProcessId proc_id = task->process_id();
  const TaskId task_id = task->task_id();
  const bool is_running_in_vm = task->IsRunningInVM();
  const bool is_running_in_lacros = task->GetType() == Task::LACROS;

  TaskManagerImpl::PidToTaskGroupMap& task_group_map =
      is_running_in_vm ? arc_vm_task_groups_by_proc_id_
                       : is_running_in_lacros ? crosapi_task_groups_by_proc_id_
                                              : task_groups_by_proc_id_;

  DCHECK(task_group_map.count(proc_id));

  NotifyObserversOnTaskToBeRemoved(task_id);

  TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  task_group->RemoveTask(task);
  task_groups_by_task_id_.erase(task_id);

  if (task_group->empty())
    task_group_map.erase(proc_id);  // Deletes |task_group|.

  // Invalidate the cached sorted IDs by clearing the list.
  sorted_task_ids_.clear();
}

void TaskManagerImpl::TaskUnresponsive(Task* task) {
  DCHECK(task);
  NotifyObserversOnTaskUnresponsive(task->task_id());
}

void TaskManagerImpl::ActiveTaskFetched(TaskId active_task_id) {
  NotifyObserversOnActiveTaskFetched(active_task_id);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TaskManagerImpl::TaskIdsListToBeInvalidated() {
  sorted_task_ids_.clear();
  NotifyObserversOnRefresh(GetTaskIdsList());
}
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

void TaskManagerImpl::UpdateAccumulatedStatsNetworkForRoute(
    content::GlobalRenderFrameHostId render_frame_host_id,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!is_running_)
    return;
  Task* task = GetTaskByRoute(render_frame_host_id);
  if (!task) {
    // Orphaned/unaccounted activity is credited to the Browser process.
    task = GetTaskByRoute(content::GlobalRenderFrameHostId());
  }
  if (!task)
    return;
  task->OnNetworkBytesRead(recv_bytes);
  task->OnNetworkBytesSent(sent_bytes);
}

void TaskManagerImpl::OnVideoMemoryUsageStatsUpdate(
    const gpu::VideoMemoryUsageStats& gpu_memory_stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gpu_memory_stats_ = gpu_memory_stats;
}

void TaskManagerImpl::OnReceivedMemoryDump(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  waiting_for_memory_dump_ = false;
  // We can ignore the value of success as it is a coarse grained indicator
  // of whether the global dump was successful; usually because of a missing
  // process or OS dumps. There may still be useful information for other
  // processes in the global dump when success is false.
  if (!dump)
    return;
  for (const auto& pmd : dump->process_dumps()) {
    auto it = task_groups_by_proc_id_.find(pmd.pid());
    if (it == task_groups_by_proc_id_.end())
      continue;
    it->second->set_footprint_bytes(
        static_cast<uint64_t>(pmd.os_dump().private_footprint_kb) * 1024);
  }
}

void TaskManagerImpl::Refresh() {
  if (IsResourceRefreshEnabled(REFRESH_TYPE_GPU_MEMORY)) {
    content::GpuDataManager::GetInstance()->RequestVideoMemoryUsageStatsUpdate(
        base::BindOnce(&TaskManagerImpl::OnVideoMemoryUsageStatsUpdate,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (IsResourceRefreshEnabled(REFRESH_TYPE_MEMORY_FOOTPRINT) &&
      !waiting_for_memory_dump_) {
    // The callback keeps this object alive until the callback is invoked.
    waiting_for_memory_dump_ = true;
    auto callback = base::BindOnce(&TaskManagerImpl::OnReceivedMemoryDump,
                                   weak_ptr_factory_.GetWeakPtr());
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->RequestPrivateMemoryFootprint(base::kNullProcessId,
                                        std::move(callback));
  }
  for (auto& groups_itr : task_groups_by_proc_id_) {
    groups_itr.second->Refresh(gpu_memory_stats_, GetCurrentRefreshTime(),
                               enabled_resources_flags());
  }

  for (auto& groups_itr : arc_vm_task_groups_by_proc_id_) {
    groups_itr.second->Refresh(gpu_memory_stats_, GetCurrentRefreshTime(),
                               enabled_resources_flags());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (auto& groups_itr : crosapi_task_groups_by_proc_id_) {
    groups_itr.second->Refresh(gpu_memory_stats_, GetCurrentRefreshTime(),
                               enabled_resources_flags());
  }

  if (TaskManagerObserver::IsResourceRefreshEnabled(
          REFRESH_TYPE_MEMORY_FOOTPRINT, enabled_resources_flags())) {
    arc_shared_sampler_->Refresh();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NotifyObserversOnRefresh(GetTaskIdsList());
}

void TaskManagerImpl::StartUpdating() {
  if (is_running_)
    return;

  is_running_ = true;

  content::GetNetworkService()->EnableDataUseUpdates(true);

  for (const auto& provider : task_providers_)
    provider->SetObserver(this);

  // Kick off fetch of asynchronous data, e.g., memory footprint, so that it
  // will be displayed sooner after opening the task manager.
  Refresh();
}

void TaskManagerImpl::StopUpdating() {
  if (!is_running_)
    return;

  is_running_ = false;

  content::GetNetworkService()->EnableDataUseUpdates(false);

  for (const auto& provider : task_providers_)
    provider->ClearObserver();

  task_groups_by_proc_id_.clear();
  arc_vm_task_groups_by_proc_id_.clear();
  crosapi_task_groups_by_proc_id_.clear();
  task_groups_by_task_id_.clear();
  sorted_task_ids_.clear();
}

Task* TaskManagerImpl::GetTaskByRoute(
    content::GlobalRenderFrameHostId render_frame_host_id) const {
  for (const auto& task_provider : task_providers_) {
    Task* task = task_provider->GetTaskOfUrlRequest(
        render_frame_host_id.child_id, render_frame_host_id.frame_routing_id);
    if (task)
      return task;
  }
  return nullptr;
}

TaskGroup* TaskManagerImpl::GetTaskGroupByTaskId(TaskId task_id) const {
  auto it = task_groups_by_task_id_.find(task_id);
  CHECK(it != task_groups_by_task_id_.end(), base::NotFatalUntil::M130);
  return it->second;
}

Task* TaskManagerImpl::GetTaskByTaskId(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->GetTaskById(task_id);
}

void TaskManagerImpl::OnTaskGroupBackgroundCalculationsDone() {
  for (const auto& groups_itr : task_groups_by_proc_id_) {
    if (!groups_itr.second->AreBackgroundCalculationsDone())
      return;
  }
  NotifyObserversOnRefreshWithBackgroundCalculations(GetTaskIdsList());
  for (const auto& groups_itr : task_groups_by_proc_id_)
    groups_itr.second->ClearCurrentBackgroundCalculationsFlags();
}

}  // namespace task_manager
