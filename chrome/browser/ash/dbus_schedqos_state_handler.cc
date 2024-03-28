// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus_schedqos_state_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "dbus/dbus_result.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

namespace ash {

DBusSchedQOSStateHandler::DBusSchedQOSStateHandler(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : main_task_runner_(main_task_runner) {
  base::Process::SetProcessPriorityDelegate(this);
}

DBusSchedQOSStateHandler::~DBusSchedQOSStateHandler() {
  base::Process::SetProcessPriorityDelegate(nullptr);
}

// static
DBusSchedQOSStateHandler* DBusSchedQOSStateHandler::Create(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner) {
  return new DBusSchedQOSStateHandler(main_task_runner);
}

bool DBusSchedQOSStateHandler::CanSetProcessPriority() {
  return true;
}

void DBusSchedQOSStateHandler::InitializeProcessPriority(
    base::ProcessId process_id) {
  // Processes in ChromeOS are Priority::kUserBlocking by default.
  base::Process::Priority default_priority =
      base::Process::Priority::kUserBlocking;
  {
    base::AutoLock lock(process_priority_map_lock_);
    process_priority_map_[process_id] = default_priority;
  }
  // It is required to set priority explicitly here because setting thread QoS
  // states requires the process QoS state in advance.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBusSchedQOSStateHandler::SetProcessPriorityOnThread,
                     weak_ptr_factory_.GetWeakPtr(), process_id,
                     default_priority));
}

void DBusSchedQOSStateHandler::ForgetProcessPriority(
    base::ProcessId process_id) {
  base::AutoLock lock(process_priority_map_lock_);
  if (auto entry = process_priority_map_.find(process_id);
      entry != process_priority_map_.end()) {
    process_priority_map_.erase(entry);
  }
}

bool DBusSchedQOSStateHandler::SetProcessPriority(
    base::ProcessId process_id,
    base::Process::Priority priority) {
  {
    // The overhead of lock should rarely be a problem because most of
    // base::Process::SetPriority() is called from
    // ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread() which is
    // single-threaded and base::Process::GetPriority() is rarely called.
    base::AutoLock lock(process_priority_map_lock_);
    if (auto entry = process_priority_map_.find(process_id);
        entry != process_priority_map_.end()) {
      entry->second = priority;
    } else {
      // Processes not registered by InitializeProcessPriority() are rejected.
      // Otherwise entries in the map leak after the processes are dead.
      LOG(ERROR) << "process " << process_id
                 << " is not initialized for priority change";
      return false;
    }
  }
  return main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBusSchedQOSStateHandler::SetProcessPriorityOnThread,
                     weak_ptr_factory_.GetWeakPtr(), process_id, priority));
}

base::Process::Priority DBusSchedQOSStateHandler::GetProcessPriority(
    base::ProcessId process_id) {
  // Processes in ChromeOS are Priority::kUserBlocking by default.
  base::Process::Priority priority = base::Process::Priority::kUserBlocking;
  {
    base::AutoLock lock(process_priority_map_lock_);
    if (auto entry = process_priority_map_.find(process_id);
        entry != process_priority_map_.end()) {
      priority = entry->second;
    }
  }
  if (priority == base::Process::Priority::kUserVisible) {
    // base::Process::GetPriority() returns either kBestEffort or kUserBlocking.
    priority = base::Process::Priority::kUserBlocking;
  }
  return priority;
}

void DBusSchedQOSStateHandler::SetProcessPriorityOnThread(
    base::ProcessId process_id,
    base::Process::Priority priority) {
  resource_manager::ProcessState state =
      resource_manager::ProcessState::kNormal;
  switch (priority) {
    case base::Process::Priority::kBestEffort:
      state = resource_manager::ProcessState::kBackground;
      break;
    case base::Process::Priority::kUserBlocking:
    case base::Process::Priority::kUserVisible:
      state = resource_manager::ProcessState::kNormal;
      break;
  }
  ash::ResourcedClient::Get()->SetProcessState(
      process_id, state,
      base::BindOnce(&DBusSchedQOSStateHandler::OnSetProcessPriorityFinish,
                     weak_ptr_factory_.GetWeakPtr(), process_id, priority));
}

void DBusSchedQOSStateHandler::OnSetProcessPriorityFinish(
    base::ProcessId process_id,
    base::Process::Priority priority,
    dbus::DBusResult result) {
  LOG_IF(ERROR, result != dbus::DBusResult::kSuccess)
      << "set process state via resourced failed for pid " << process_id
      << " to " << static_cast<int>(priority)
      << " : DBusResult: " << static_cast<int>(result);
}

}  // namespace ash
