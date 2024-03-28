// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus_schedqos_state_handler.h"

#include <vector>

#include "base/check.h"
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

DBusSchedQOSStateHandler::ProcessState::ProcessState(
    base::Process::Priority priority)
    : priority(priority) {}

DBusSchedQOSStateHandler::ProcessState::~ProcessState() = default;
DBusSchedQOSStateHandler::ProcessState::ProcessState(ProcessState&&) = default;

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
  auto* handler = new DBusSchedQOSStateHandler(main_task_runner);

  ash::ResourcedClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&DBusSchedQOSStateHandler::OnServiceConnected,
                     handler->weak_ptr_factory_.GetWeakPtr()));

  return handler;
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
    base::AutoLock lock(process_state_map_lock_);
    process_state_map_.emplace(process_id, ProcessState(default_priority));
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
  base::AutoLock lock(process_state_map_lock_);
  if (auto entry = process_state_map_.find(process_id);
      entry != process_state_map_.end()) {
    process_state_map_.erase(entry);
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
    base::AutoLock lock(process_state_map_lock_);
    if (auto entry = process_state_map_.find(process_id);
        entry != process_state_map_.end()) {
      entry->second.priority = priority;
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
    base::AutoLock lock(process_state_map_lock_);
    if (auto entry = process_state_map_.find(process_id);
        entry != process_state_map_.end()) {
      priority = entry->second.priority;
    }
  }
  if (priority == base::Process::Priority::kUserVisible) {
    // base::Process::GetPriority() returns either kBestEffort or kUserBlocking.
    priority = base::Process::Priority::kUserBlocking;
  }
  return priority;
}

void DBusSchedQOSStateHandler::OnServiceConnected(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(ERROR) << "resourced service is not available";
    return;
  }

  DCHECK(!is_connected_);
  if (is_connected_) {
    LOG(ERROR)
        << "DBusSchedQOSStateHandler::OnServiceConnected while it is connected";
    return;
  }
  is_connected_ = true;

  std::vector<std::pair<base::ProcessId, base::Process::Priority>>
      preconnect_requests;
  // Copy entries in process_state_map_ to local vector to minimize the
  // locked section.
  // SetProcessPriority() calls just before/after this locked section do not
  // cause incoherence because both OnServiceConnected() and
  // SetProcessPriorityOnThread() run on the same main thread.
  {
    base::AutoLock lock(process_state_map_lock_);
    preconnect_requests.reserve(process_state_map_.size());
    for (auto& entry : process_state_map_) {
      if (entry.second.need_retry) {
        preconnect_requests.push_back({entry.first, entry.second.priority});
        entry.second.need_retry = false;
      }
    }
  }

  for (const auto& request : preconnect_requests) {
    SetProcessPriorityOnThread(request.first, request.second);
  }
}

void DBusSchedQOSStateHandler::SetProcessPriorityOnThread(
    base::ProcessId process_id,
    base::Process::Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_connected_) {
    MarkProcessToRetry(process_id);
    return;
  }

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG_IF(ERROR, result != dbus::DBusResult::kSuccess)
      << "set process state via resourced failed for pid " << process_id
      << " to " << static_cast<int>(priority)
      << " : DBusResult: " << static_cast<int>(result);

  if (is_connected_ && result == dbus::DBusResult::kErrorServiceUnknown) {
    is_connected_ = false;
    ash::ResourcedClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&DBusSchedQOSStateHandler::OnServiceConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (!is_connected_) {
    MarkProcessToRetry(process_id);
  }
}

void DBusSchedQOSStateHandler::MarkProcessToRetry(base::ProcessId process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(process_state_map_lock_);
  if (auto entry = process_state_map_.find(process_id);
      entry != process_state_map_.end()) {
    entry->second.need_retry = true;
  }
}

}  // namespace ash
