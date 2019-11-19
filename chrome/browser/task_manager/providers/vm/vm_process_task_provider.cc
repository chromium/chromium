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
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/providers/vm/crostini_process_task.h"
#include "chrome/browser/task_manager/providers/vm/plugin_vm_process_task.h"
#include "content/public/browser/browser_thread.h"

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
    if (base::EndsWith(filename, ext, base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

// The argument this is extracting from will look like this:
// /home/root/53d63eda33c610d37b44cde8ed06854a05e9cc84/crosvm/dGVybWluYQ==.qcow2
// This is the path to a VM disk in the user-specific root directory that is not
// exposed to the user in the Files app. The base for this is /home/root and
// the cryptohome ID for the user is the next path element. Then we have the
// service specific 'crosvm' directory which we put VM images in. VM images use
// base64 encoding of the VM name as the filename with a .qcow2/.img extension.
bool CrostiniExtractVmNameAndOwnerId(const std::string& arg,
                                     std::string* vm_name_out,
                                     std::string* owner_id_out) {
  DCHECK(vm_name_out);
  DCHECK(owner_id_out);

  // All VM disk images are contained in a subdirectory of this path.
  constexpr char kVmDiskRoot[] = "/home/root/";

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

  // The owner ID is the long hex string in there...which is 2 parents up.
  // It's safe to call this even if there's not enough parents because the
  // DirName of the root is still the root.
  *owner_id_out = vm_disk_path.DirName().DirName().BaseName().value();

  return true;
}

// We are looking for argument like this:
// /run/pvm-images/<cryptohome id>/UHZtRGVmYXVsdA==.pvm/...
bool PluginVmExtractVmNameAndOwnerId(const std::string& arg,
                                     std::string* vm_name_out,
                                     std::string* owner_id_out) {
  DCHECK(vm_name_out);
  DCHECK(owner_id_out);

  // All VM disk images are mounted at this path.
  constexpr char kVmDiskRoot[] = "/run/pvm-images/";

  // Skip paths that don't start with the correct prefix.
  if (!base::StartsWith(arg, kVmDiskRoot, base::CompareCase::SENSITIVE)) {
    return false;
  }

  const base::FilePath vm_disk_path(arg);

  std::vector<std::string> components;
  vm_disk_path.GetComponents(&components);

  // Expect /, root, pvm-images, <owner_id>, vm_name.pvm, ...
  if (components.size() < 5)
    return false;

  base::FilePath vm_subdir(components[4]);
  if (vm_subdir.Extension() != ".pvm")
    return false;

  // The VM name is the base64 encoded string which is the name of the
  // file itself without the extension.
  base::Base64Decode(vm_subdir.RemoveExtension().value(), vm_name_out);

  // The owner ID is the long hex string in there...which is 2 parents up.
  // It's safe to call this even if there's not enough parents because the
  // DirName of the root is still the root.
  *owner_id_out = components[3];

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
  for (const auto arg : cmdline) {
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
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_VISIBLE})),
      refresh_timer_(
          FROM_HERE,
          kRefreshProcessListDelay,
          base::BindRepeating(&VmProcessTaskProvider::RequestVmProcessList,
                              base::Unretained(this))) {}

VmProcessTaskProvider::~VmProcessTaskProvider() = default;

Task* VmProcessTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  // VM tasks are not associated with any URL request.
  return nullptr;
}

void VmProcessTaskProvider::StartUpdating() {
  RequestVmProcessList();
  refresh_timer_.Reset();
}

void VmProcessTaskProvider::StopUpdating() {
  refresh_timer_.Stop();
  task_map_.clear();
}

std::vector<VmProcessData> GetVmProcessList() {
  std::vector<VmProcessData> ret_processes;
  const base::ProcessIterator::ProcessEntries& entry_list =
      base::ProcessIterator(nullptr).Snapshot();
  const base::ProcessId vm_init_pid = GetVmInitProcessId(entry_list);
  if (vm_init_pid == base::kNullProcessId) {
    return ret_processes;
  }

  // Find all of the child processes of vm_concierge, the ones that are the
  // crosvm program are the VM processes, we can then extract the name of the
  // VM from its command line args.
  for (const base::ProcessEntry& entry : entry_list) {
    if (entry.parent_pid() == vm_init_pid && !entry.cmd_line_args().empty() &&
        entry.cmd_line_args()[0] == kVmProcessName) {
      std::string vm_name;
      std::string owner_id;
      bool is_plugin_vm;
      if (ExtractVmNameAndOwnerIdFromCmdLine(entry.cmd_line_args(), &vm_name,
                                             &owner_id, &is_plugin_vm))
        ret_processes.emplace_back(vm_name, owner_id, entry.pid(),
                                   is_plugin_vm);
    }
  }
  return ret_processes;
}

void VmProcessTaskProvider::RequestVmProcessList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&GetVmProcessList),
      base::BindOnce(&VmProcessTaskProvider::OnUpdateVmProcessList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmProcessTaskProvider::OnUpdateVmProcessList(
    const std::vector<VmProcessData>& vm_process_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!refresh_timer_.IsRunning())
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
