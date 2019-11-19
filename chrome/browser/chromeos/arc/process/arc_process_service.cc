// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The main point of this class is to cache ARC proc nspid<->pid mapping
// globally. Since the calculation is costly, a dedicated worker thread is
// used. All read/write of its internal data structure (i.e., the mapping)
// should be on this thread.

#include "chrome/browser/chromeos/arc/process/arc_process_service.h"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

using base::kNullProcessId;
using base::Process;
using base::ProcessId;
using std::vector;

namespace {

static constexpr char kInitName[] = "/init";
static constexpr bool kNotFocused = false;
static constexpr int64_t kNoActivityTimeInfo = 0L;

// Matches the process name "/init" in the process tree and get the
// corresponding process ID.
base::ProcessId GetArcInitProcessId(
    const base::ProcessIterator::ProcessEntries& entry_list) {
  for (const base::ProcessEntry& entry : entry_list) {
    if (entry.cmd_line_args().empty()) {
      continue;
    }
    // TODO(nya): Add more constraints to avoid mismatches.
    const std::string& process_name = entry.cmd_line_args()[0];
    if (process_name == kInitName) {
      return entry.pid();
    }
  }
  return base::kNullProcessId;
}

std::vector<ArcProcess> GetArcSystemProcessList() {
  std::vector<ArcProcess> ret_processes;

  if (arc::IsArcVmEnabled()) {
    // TODO(b/122992194): Fix this for ARCVM.
    return ret_processes;
  }

  const base::ProcessIterator::ProcessEntries& entry_list =
      base::ProcessIterator(nullptr).Snapshot();
  const base::ProcessId arc_init_pid = GetArcInitProcessId(entry_list);

  if (arc_init_pid == base::kNullProcessId) {
    return ret_processes;
  }

  // Enumerate the child processes of ARC init for gathering ARC System
  // Processes.
  for (const base::ProcessEntry& entry : entry_list) {
    if (entry.cmd_line_args().empty()) {
      continue;
    }
    // TODO(hctsai): For now, we only gather direct child process of init, need
    // to get the processes below. For example, installd might fork dex2oat and
    // it can be executed for minutes.
    if (entry.parent_pid() == arc_init_pid) {
      const base::ProcessId child_pid = entry.pid();
      const base::ProcessId child_nspid =
          base::Process(child_pid).GetPidInNamespace();
      if (child_nspid != base::kNullProcessId) {
        const std::string& process_name = entry.cmd_line_args()[0];
        // The is_focused and last_activity_time is not needed thus mocked
        ret_processes.emplace_back(child_nspid, child_pid, process_name,
                                   mojom::ProcessState::PERSISTENT, kNotFocused,
                                   kNoActivityTimeInfo);
      }
    }
  }
  return ret_processes;
}

void UpdateNspidToPidMap(
    scoped_refptr<ArcProcessService::NSPidToPidMap> pid_map) {
  TRACE_EVENT0("browser", "ArcProcessService::UpdateNspidToPidMap");

  // NB: Despite of its name, ProcessIterator::Snapshot() may return
  // inconsistent information because it simply walks procfs. Especially
  // we must not assume the parent-child relationships are consistent.
  const base::ProcessIterator::ProcessEntries& entry_list =
      base::ProcessIterator(nullptr).Snapshot();

  // Construct the process tree.
  // NB: This can contain a loop in case of race conditions.
  std::unordered_map<ProcessId, std::vector<ProcessId>> process_tree;
  for (const base::ProcessEntry& entry : entry_list)
    process_tree[entry.parent_pid()].push_back(entry.pid());

  ProcessId arc_init_pid = GetArcInitProcessId(entry_list);

  // Enumerate all processes under ARC init and create nspid -> pid map.
  if (arc_init_pid != kNullProcessId) {
    base::queue<ProcessId> queue;
    std::unordered_set<ProcessId> visited;
    queue.push(arc_init_pid);
    while (!queue.empty()) {
      ProcessId pid = queue.front();
      queue.pop();
      // Do not visit the same process twice. Otherwise we may enter an infinite
      // loop if |process_tree| contains a loop.
      if (!visited.insert(pid).second)
        continue;

      const ProcessId nspid = base::Process(pid).GetPidInNamespace();

      // All ARC processes should be in namespace so nspid is usually non-null,
      // but this can happen if the process has already gone.
      // Only add processes we're interested in (those appear as keys in
      // |pid_map|).
      if (nspid != kNullProcessId && pid_map->find(nspid) != pid_map->end())
        (*pid_map)[nspid] = pid;

      for (ProcessId child_pid : process_tree[pid])
        queue.push(child_pid);
    }
  }
}

std::vector<ArcProcess> FilterProcessList(
    const ArcProcessService::NSPidToPidMap& pid_map,
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  std::vector<ArcProcess> ret_processes;
  for (const auto& entry : processes) {
    base::ProcessId pid;
    if (arc::IsArcVmEnabled()) {
      // When VM is enabled, there is no external pid. Set the pid here to the
      // guest pid. Setting the pid to zero was considered but the task manager
      // groups tasks by pid and this can cause incorrect aggregated stats to
      // be displayed. The task manager will handle these cases by checking if
      // the task is in VM (via Task::IsRunningInVM) and know to partition off
      // these processes.
      pid = entry->pid;
    } else {
      const auto it = pid_map.find(entry->pid);
      // The nspid could be missing due to race condition. For example, the
      // process is still running when we get the process snapshot and ends when
      // we update the nspid to pid mapping.
      if (it == pid_map.end() || it->second == base::kNullProcessId) {
        continue;
      }
      pid = it->second;
    }
    // Constructs the ArcProcess instance if the mapping is found.
    ArcProcess arc_process(entry->pid, pid, entry->process_name,
                           entry->process_state, entry->is_focused,
                           entry->last_activity_time);
    // |entry->packages| is provided only when process.mojom's verion is >=4.
    if (entry->packages) {
      for (const auto& package : *entry->packages) {
        arc_process.packages().push_back(package);
      }
    }
    ret_processes.push_back(std::move(arc_process));
  }
  return ret_processes;
}

std::vector<ArcProcess> UpdateAndReturnProcessList(
    scoped_refptr<ArcProcessService::NSPidToPidMap> nspid_map,
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  ArcProcessService::NSPidToPidMap& pid_map = *nspid_map;
  if (!arc::IsArcVmEnabled()) {
    // Cleanup dead pids in the cache |pid_map|.
    std::unordered_set<ProcessId> nspid_to_remove;
    for (const auto& entry : pid_map) {
      nspid_to_remove.insert(entry.first);
    }
    bool unmapped_nspid = false;
    for (const auto& entry : processes) {
      // erase() returns 0 if coudln't find the key. It means a new process.
      if (nspid_to_remove.erase(entry->pid) == 0) {
        pid_map[entry->pid] = base::kNullProcessId;
        unmapped_nspid = true;
      }
    }
    for (const auto& entry : nspid_to_remove) {
      pid_map.erase(entry);
    }

    // The operation is costly so avoid calling it when possible.
    if (unmapped_nspid) {
      UpdateNspidToPidMap(nspid_map);
    }
  }

  return FilterProcessList(pid_map, std::move(processes));
}

std::vector<mojom::ArcMemoryDumpPtr> UpdateAndReturnMemoryInfo(
    scoped_refptr<ArcProcessService::NSPidToPidMap> nspid_map,
    std::vector<mojom::ArcMemoryDumpPtr> process_dumps) {
  if (!arc::IsArcVmEnabled()) {
    ArcProcessService::NSPidToPidMap& pid_map = *nspid_map;
    // Cleanup dead processes in pid_map
    base::flat_set<ProcessId> nspid_to_remove;
    for (const auto& entry : pid_map)
      nspid_to_remove.insert(entry.first);

    bool unmapped_nspid = false;
    for (const auto& proc : process_dumps) {
      // erase() returns 0 if couldn't find the key (new process)
      if (nspid_to_remove.erase(proc->pid) == 0) {
        pid_map[proc->pid] = base::kNullProcessId;
        unmapped_nspid = true;
      }
    }
    for (const auto& old_nspid : nspid_to_remove)
      pid_map.erase(old_nspid);

    if (unmapped_nspid)
      UpdateNspidToPidMap(nspid_map);

    // Return memory info only for processes that have a mapping nspid->pid
    for (auto& proc : process_dumps) {
      auto it = pid_map.find(proc->pid);
      proc->pid = it == pid_map.end() ? kNullProcessId : it->second;
    }
    base::EraseIf(process_dumps,
                  [](const auto& proc) { return proc->pid == kNullProcessId; });
  }
  return process_dumps;
}

void Reset(scoped_refptr<ArcProcessService::NSPidToPidMap> pid_map) {
  if (pid_map.get())
    pid_map->clear();
}

// Singleton factory for ArcProcessService.
class ArcProcessServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcProcessService,
          ArcProcessServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcProcessServiceFactory";

  static ArcProcessServiceFactory* GetInstance() {
    return base::Singleton<ArcProcessServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcProcessServiceFactory>;
  ArcProcessServiceFactory() = default;
  ~ArcProcessServiceFactory() override = default;
};

}  // namespace

// static
ArcProcessService* ArcProcessService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcProcessServiceFactory::GetForBrowserContext(context);
}

ArcProcessService::ArcProcessService(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), nspid_to_pid_(new NSPidToPidMap()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE});
  arc_bridge_service_->process()->AddObserver(this);
}

ArcProcessService::~ArcProcessService() {
  arc_bridge_service_->process()->RemoveObserver(this);
}

// static
ArcProcessService* ArcProcessService::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This is called from TaskManager implementation, which is isolated
  // from BrowserContext.
  // Use ArcServiceManager's BrowserContext instance, since 1) it is always
  // allowed to use ARC, and 2) the rest of ARC service's lifetime are
  // tied to it.
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager || !arc_service_manager->browser_context())
    return nullptr;
  return GetForBrowserContext(arc_service_manager->browser_context());
}

void ArcProcessService::RequestSystemProcessList(
    RequestProcessListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(task_runner_.get(), FROM_HERE,
                                   base::BindOnce(&GetArcSystemProcessList),
                                   std::move(callback));
}

void ArcProcessService::RequestAppProcessList(
    RequestProcessListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Since several services call this class to get information about the ARC
  // process list, it can produce a lot of logspam when the board is ARC-ready
  // but the user has not opted into ARC. This redundant check avoids that
  // logspam.
  if (!connection_ready_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestProcessList);
  if (!process_instance) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  process_instance->RequestProcessList(
      base::BindOnce(&ArcProcessService::OnReceiveProcessList,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ArcProcessService::RequestAppMemoryInfo(
    RequestMemoryInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!connection_ready_)
    return false;

  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestApplicationProcessMemoryInfo);
  if (!process_instance) {
    LOG(ERROR) << "could not find method / get ProcessInstance";
    return false;
  }
  process_instance->RequestApplicationProcessMemoryInfo(
      base::BindOnce(&ArcProcessService::OnReceiveMemoryInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

bool ArcProcessService::RequestSystemMemoryInfo(
    RequestMemoryInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!connection_ready_)
    return false;
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&GetArcSystemProcessList),
      base::BindOnce(&ArcProcessService::OnGetSystemProcessList,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcProcessService::OnReceiveProcessList(
    RequestProcessListCallback callback,
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&UpdateAndReturnProcessList, nspid_to_pid_,
                     std::move(processes)),
      std::move(callback));
}

void ArcProcessService::OnReceiveMemoryInfo(
    RequestMemoryInfoCallback callback,
    std::vector<mojom::ArcMemoryDumpPtr> process_dumps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&UpdateAndReturnMemoryInfo, nspid_to_pid_,
                     std::move(process_dumps)),
      std::move(callback));
}

void ArcProcessService::OnGetSystemProcessList(
    RequestMemoryInfoCallback callback,
    std::vector<ArcProcess> procs) {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestSystemProcessMemoryInfo);
  if (!process_instance) {
    LOG(ERROR) << "could not find method / get ProcessInstance";
    return;
  }
  std::vector<uint32_t> nspids;
  if (!arc::IsArcVmEnabled()) {
    for (const auto& proc : procs)
      nspids.push_back(proc.nspid());
  }
  // TODO(b/122992194): Fix this for ARCVM.
  process_instance->RequestSystemProcessMemoryInfo(
      nspids,
      base::BindOnce(&ArcProcessService::OnReceiveMemoryInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcProcessService::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&Reset, nspid_to_pid_));
  connection_ready_ = true;
}

void ArcProcessService::OnConnectionClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  connection_ready_ = false;
}

inline ArcProcessService::NSPidToPidMap::NSPidToPidMap() {}

inline ArcProcessService::NSPidToPidMap::~NSPidToPidMap() {}

}  // namespace arc
