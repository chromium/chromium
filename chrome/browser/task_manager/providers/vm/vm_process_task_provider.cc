// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/vm/vm_process_task_provider.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/providers/vm/crostini_process_task.h"
#include "chrome/browser/task_manager/providers/vm/plugin_vm_process_task.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/process_snapshot/process_snapshot_server.h"

namespace task_manager {

namespace {

// This is the binary for the process which is the parent process of all the
// VMs.
constexpr char kVmConciergeName[] = "/usr/bin/vm_concierge";

// This is the binary executed for a VM process.
constexpr char kVmProcessName[] = "/usr/bin/crosvm";

// Delay between refreshing the list of VM processes.
constexpr base::TimeDelta kRefreshProcessListDelay = base::Seconds(5);

// Matches the process name "vm_concierge" in the process tree and get the
// corresponding process ID.
base::ProcessId GetVmInitProcessId(
    const base::ProcessIterator::ProcessEntries& entry_list) {
  for (const base::ProcessEntry& entry : entry_list) {
    if (!entry.cmd_line_args().empty() &&
        entry.cmd_line_args()[0] == kVmConciergeName) {
      return entry.pid();
    }
  }
  return base::kNullProcessId;
}

ash::ConciergeClient* GetConciergeClient() {
  return ash::ConciergeClient::Get();
}

}  // namespace

struct VmProcessData {
  VmProcessData(const std::string& name,
                const std::string& owner_id,
                const base::ProcessId& proc_id,
                bool plugin_vm)
      : vm_name(name),
        owner_id(owner_id),
        pid(proc_id),
        is_plugin_vm(plugin_vm) {}
  std::string vm_name;
  std::string owner_id;
  base::ProcessId pid;
  bool is_plugin_vm;
};

VmProcessTaskProvider::VmProcessTaskProvider()
    : ash::ProcessSnapshotServer::Observer(kRefreshProcessListDelay) {}

VmProcessTaskProvider::~VmProcessTaskProvider() = default;

Task* VmProcessTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  // VM tasks are not associated with any URL request.
  return nullptr;
}

void VmProcessTaskProvider::OnProcessSnapshotRefreshed(
    const base::ProcessIterator::ProcessEntries& snapshot) {
  TRACE_EVENT0("browser", "VmProcessTaskProvider::OnProcessSnapshotRefreshed");

  // Throttle the refreshes in case the `ash::ProcessSnapshotServer` has
  // observers with a much higher desired refresh rates.
  const auto old_snapshot_time = last_process_snapshot_time_;
  last_process_snapshot_time_ = base::Time::Now();
  if ((last_process_snapshot_time_ - old_snapshot_time) <
      kRefreshProcessListDelay) {
    return;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile) {
    const std::string active_owner_id =
        crostini::CryptohomeIdForProfile(profile);
    vm_tools::concierge::ListVmsRequest request;
    request.set_owner_id(active_owner_id);
    GetConciergeClient()->ListVms(
        request, base::BindOnce(&VmProcessTaskProvider::OnListVms,
                                weak_ptr_factory_.GetWeakPtr(), snapshot));
  }
}

void VmProcessTaskProvider::OnListVms(
    const base::ProcessIterator::ProcessEntries& snapshot,
    std::optional<vm_tools::concierge::ListVmsResponse> response) {
  std::vector<VmProcessData> vm_process_list;
  const base::ProcessId vm_init_pid = GetVmInitProcessId(snapshot);

  if (vm_init_pid == base::kNullProcessId || !response.has_value()) {
    OnUpdateVmProcessList(vm_process_list);
    return;
  }

  std::map</*pid=*/int, const vm_tools::concierge::ExtendedVmInfo*> vms;
  for (const auto& vm : response.value().vms()) {
    vms[vm.vm_info().pid()] = &vm;
  }

  // Find all of the child processes of vm_concierge, the ones that are having
  // matching namespaced pid in vms are VM processes
  for (const base::ProcessEntry& entry : snapshot) {
    if (entry.parent_pid() == vm_init_pid && !entry.cmd_line_args().empty() &&
        entry.cmd_line_args()[0] == kVmProcessName) {
      auto nspid = base::Process(entry.pid()).GetPidInNamespace();
      auto vm_entry = vms.find(nspid);
      if (vm_entry != vms.end()) {
        auto* vm = vm_entry->second;
        vm_process_list.emplace_back(
            vm->name(), vm->owner_id(), entry.pid(),
            vm->vm_info().vm_type() ==
                vm_tools::concierge::VmInfo_VmType_PLUGIN_VM);
      }
    }
  }

  OnUpdateVmProcessList(vm_process_list);
}

void VmProcessTaskProvider::StartUpdating() {
  ash::ProcessSnapshotServer::Get()->AddObserver(this);
}

void VmProcessTaskProvider::StopUpdating() {
  ash::ProcessSnapshotServer::Get()->RemoveObserver(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_map_.clear();
}

void VmProcessTaskProvider::OnUpdateVmProcessList(
    const std::vector<VmProcessData>& vm_process_list) {
  if (!IsUpdating())
    return;

  base::flat_set<base::ProcessId> pids_to_remove;
  for (const auto& entry : task_map_)
    pids_to_remove.insert(entry.first);

  // Get the cryptohome ID for the active profile and ensure we only list VMs
  // that are associated with it.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile) {
    const std::string active_owner_id =
        crostini::CryptohomeIdForProfile(profile);
    for (const auto& entry : vm_process_list) {
      if (active_owner_id != entry.owner_id)
        continue;
      if (pids_to_remove.erase(entry.pid))
        continue;

      // New VM process.
      if (entry.is_plugin_vm) {
        auto task = std::make_unique<PluginVmProcessTask>(
            entry.pid, entry.owner_id, entry.vm_name);
        task_map_[entry.pid] = std::move(task);
      } else {
        auto task = std::make_unique<CrostiniProcessTask>(
            entry.pid, entry.owner_id, entry.vm_name);
        task_map_[entry.pid] = std::move(task);
      }
      NotifyObserverTaskAdded(task_map_[entry.pid].get());
    }
  }

  for (const auto& entry : pids_to_remove) {
    // Stale VM process.
    auto iter = task_map_.find(entry);
    NotifyObserverTaskRemoved(iter->second.get());
    task_map_.erase(iter);
  }
}

}  // namespace task_manager
