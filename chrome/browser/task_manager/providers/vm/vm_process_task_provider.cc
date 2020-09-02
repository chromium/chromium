// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/vm/vm_process_task_provider.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/process/process_iterator.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/process_snapshot_server.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/providers/vm/crostini_process_task.h"
#include "chrome/browser/task_manager/providers/vm/plugin_vm_process_task.h"

namespace task_manager {

namespace {

// This is the binary for the process which is the parent process of all the
// VMs.
constexpr char kVmConciergeName[] = "/usr/bin/vm_concierge";

// This is the binary executed for a VM process.
constexpr char kVmProcessName[] = "/usr/bin/crosvm";

// Delay between refreshing the list of VM processes.
constexpr base::TimeDelta kRefreshProcessListDelay =
    base::TimeDelta::FromSeconds(5);

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

// Check for the possible suffixes on VM disk names. The actual name before
// the suffix will be the base64 encoded name of the VM itself.
bool HasValidVmDiskExtension(const std::string& filename) {
  constexpr const char* valid_extensions[] = {
      ".qcow2",
      ".img",
  };

  for (auto* const ext : valid_extensions) {
    if (filename.find(ext) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// The argument this is extracting from will look like this:
// /run/daemon-store/crosvm/53d63eda33c610d37b44cde8ed06854a05e9cc84/dGVybWluYQ==.img
// This is the path to a VM disk in the user's cryptohome that's not exposed to
// the user in the Files app, consisting of the path to crosvm's daemon store,
// the user hash, then the base64 encoded VM name with a .qcow2/.img extension.
bool CrostiniExtractVmNameAndOwnerId(const std::string& arg,
                                     std::string* vm_name_out,
                                     std::string* owner_id_out) {
  DCHECK(vm_name_out);
  DCHECK(owner_id_out);

  // All VM disk images are contained in a subdirectory of this path.
  constexpr char kVmDiskRoot[] = "/run/daemon-store";

  // Skip paths that don't start with the correct prefix to filter out the
  // rootfs .img file.
  if (!base::StartsWith(arg, kVmDiskRoot, base::CompareCase::SENSITIVE)) {
    return false;
  }

  if (!HasValidVmDiskExtension(arg)) {
    return false;
  }

  const base::FilePath vm_disk_path(arg);
  // The VM name is the base64 encoded string which is the name of the
  // file itself without the extension.
  base::Base64Decode(vm_disk_path.RemoveExtension().BaseName().value(),
                     vm_name_out);

  // The owner ID is the long hex string in there...which is 1 parent up.
  // It's safe to call this even if there's not enough parents because the
  // DirName of the root is still the root.
  *owner_id_out = vm_disk_path.DirName().BaseName().value();

  return true;
}

// We are looking for an argument like this:
// /run/daemon-store/pvm/<cryptohome id>/UHZtRGVmYXVsdA==.pvm:/pvm:true
bool PluginVmExtractVmNameAndOwnerId(const std::string& arg,
                                     std::string* vm_name_out,
                                     std::string* owner_id_out) {
  DCHECK(vm_name_out);
  DCHECK(owner_id_out);

  constexpr char kArgStart[] = "/run/daemon-store/pvm/";
  constexpr char kArgEnd[] = ":/pvm:true";

  // Skip paths that don't start/end with the expected prefix/suffix.
  if (!base::StartsWith(arg, kArgStart, base::CompareCase::SENSITIVE))
    return false;
  if (!base::EndsWith(arg, kArgEnd, base::CompareCase::SENSITIVE))
    return false;

  const base::FilePath vm_disk_path(
      base::StringPiece(arg.begin(), arg.end() - strlen(kArgEnd)));

  std::vector<std::string> components;
  vm_disk_path.GetComponents(&components);

  // Expect /, run, daemon-store, pvm, <owner_id>, vm_name.pvm
  if (components.size() != 6)
    return false;

  base::FilePath vm_subdir(components[5]);
  if (vm_subdir.Extension() != ".pvm")
    return false;

  // The VM name is the base64 encoded string which is the name of the
  // file itself without the extension.
  base::Base64Decode(vm_subdir.RemoveExtension().value(), vm_name_out);

  *owner_id_out = components[4];

  return true;
}

// This function attempts to identify if a process corresponds to a
// Crostini or Plugin VM instance by analyzing its command line arguments
// and extract owner ID and VM name from the arguments.
bool ExtractVmNameAndOwnerIdFromCmdLine(const std::vector<std::string>& cmdline,
                                        std::string* vm_name_out,
                                        std::string* owner_id_out,
                                        bool* is_plugin_vm_out) {
  DCHECK(vm_name_out);
  DCHECK(owner_id_out);
  DCHECK(is_plugin_vm_out);

  // Find the arg with the disk file path on it.
  for (const auto& arg : cmdline) {
    if (CrostiniExtractVmNameAndOwnerId(arg, vm_name_out, owner_id_out)) {
      *is_plugin_vm_out = false;
      return true;
    }
    if (PluginVmExtractVmNameAndOwnerId(arg, vm_name_out, owner_id_out)) {
      *is_plugin_vm_out = true;
      return true;
    }
  }

  return false;
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
    : ProcessSnapshotServer::Observer(kRefreshProcessListDelay) {}

VmProcessTaskProvider::~VmProcessTaskProvider() = default;

Task* VmProcessTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  // VM tasks are not associated with any URL request.
  return nullptr;
}

void VmProcessTaskProvider::OnProcessSnapshotRefreshed(
    const base::ProcessIterator::ProcessEntries& snapshot) {
  TRACE_EVENT0("browser", "VmProcessTaskProvider::OnProcessSnapshotRefreshed");

  // Throttle the refreshes in case the ProcessSnapshotServer has observers with
  // a much higher desired refresh rates.
  const auto old_snapshot_time = last_process_snapshot_time_;
  last_process_snapshot_time_ = base::Time::Now();
  if ((last_process_snapshot_time_ - old_snapshot_time) <
      kRefreshProcessListDelay) {
    return;
  }

  std::vector<VmProcessData> vm_process_list;
  const base::ProcessId vm_init_pid = GetVmInitProcessId(snapshot);
  if (vm_init_pid == base::kNullProcessId) {
    OnUpdateVmProcessList(vm_process_list);
    return;
  }

  // Find all of the child processes of vm_concierge, the ones that are the
  // crosvm program are the VM processes, we can then extract the name of the
  // VM from its command line args.
  for (const base::ProcessEntry& entry : snapshot) {
    if (entry.parent_pid() == vm_init_pid && !entry.cmd_line_args().empty() &&
        entry.cmd_line_args()[0] == kVmProcessName) {
      std::string vm_name;
      std::string owner_id;
      bool is_plugin_vm;
      if (ExtractVmNameAndOwnerIdFromCmdLine(entry.cmd_line_args(), &vm_name,
                                             &owner_id, &is_plugin_vm)) {
        vm_process_list.emplace_back(vm_name, owner_id, entry.pid(),
                                     is_plugin_vm);
      }
    }
  }

  OnUpdateVmProcessList(vm_process_list);
}

void VmProcessTaskProvider::StartUpdating() {
  ProcessSnapshotServer::Get()->AddObserver(this);
}

void VmProcessTaskProvider::StopUpdating() {
  ProcessSnapshotServer::Get()->RemoveObserver(this);
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
