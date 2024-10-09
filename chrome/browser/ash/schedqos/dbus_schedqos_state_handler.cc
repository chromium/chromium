// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/schedqos/dbus_schedqos_state_handler.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/system/procfs_util.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "dbus/dbus_result.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

namespace ash {

namespace {

constexpr base::TimeDelta kRetryServiceMonitorInterval = base::Seconds(10);

DBusSchedQOSStateHandler::PidReuseResult GetPidReuseResult(bool is_pid_reused,
                                                           bool success) {
  if (success) {
    return is_pid_reused
               ? DBusSchedQOSStateHandler::PidReuseResult::kPidReuseOnSuccess
               : DBusSchedQOSStateHandler::PidReuseResult::
                     kNotPidReuseOnSuccess;
  }
  return is_pid_reused
             ? DBusSchedQOSStateHandler::PidReuseResult::kPidReuseOnFail
             : DBusSchedQOSStateHandler::PidReuseResult::kNotPidReuseOnFail;
}

bool IsPidReused(system::ProcStatFile stat_file,
                 base::ProcessId pid,
                 bool success) {
  if (success && !stat_file.IsValid()) {
    // If the stat file does not exist before sending the request but the
    // request succeeds, it means the process/thread is dead and it applies
    // request to another process/thread with the same pid.
    return true;
  }

  return (!stat_file.IsPidAlive() && system::ProcStatFile(pid).IsValid());
}
}  // namespace

DBusSchedQOSStateHandler::ProcessState::ProcessState(
    base::Process::Priority priority)
    : ProcessState(priority, false) {}

DBusSchedQOSStateHandler::ProcessState::ProcessState(
    base::Process::Priority priority,
    bool need_retry)
    : priority(priority), need_retry(need_retry) {}

DBusSchedQOSStateHandler::ProcessState::~ProcessState() = default;
DBusSchedQOSStateHandler::ProcessState::ProcessState(ProcessState&&) = default;

DBusSchedQOSStateHandler::DBusSchedQOSStateHandler(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : main_task_runner_(main_task_runner) {
  base::Process::SetProcessPriorityDelegate(this);
  base::PlatformThread::SetThreadTypeDelegate(this);
  base::PlatformThread::SetCrossProcessPlatformThreadDelegate(this);
}

DBusSchedQOSStateHandler::~DBusSchedQOSStateHandler() {
  base::PlatformThread::SetCrossProcessPlatformThreadDelegate(nullptr);
  base::PlatformThread::SetThreadTypeDelegate(nullptr);
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
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBusSchedQOSStateHandler::SetThreadTypeOnThread,
                     weak_ptr_factory_.GetWeakPtr(), process_id, process_id,
                     base::ThreadType::kDefault));
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

bool DBusSchedQOSStateHandler::HandleThreadTypeChange(
    base::ProcessId process_id,
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  {
    base::AutoLock lock(process_state_map_lock_);
    if (auto entry = process_state_map_.find(process_id);
        entry == process_state_map_.end()) {
      LOG(ERROR) << "process " << process_id
                 << " is not initialized for thread type change for thread "
                 << thread_id;
      return false;
    }
  }
  return main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBusSchedQOSStateHandler::SetThreadTypeOnThread,
                     weak_ptr_factory_.GetWeakPtr(), process_id, thread_id,
                     thread_type));
}

bool DBusSchedQOSStateHandler::HandleThreadTypeChange(
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  return HandleThreadTypeChange(getpid(), thread_id, thread_type);
}

void DBusSchedQOSStateHandler::WaitForResourcedAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::ResourcedClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&DBusSchedQOSStateHandler::OnServiceConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DBusSchedQOSStateHandler::CheckResourcedDisconnected(
    dbus::DBusResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_connected_ && result == dbus::DBusResult::kErrorServiceUnknown) {
    is_connected_ = false;
    WaitForResourcedAvailable();
  }
}

void DBusSchedQOSStateHandler::OnServiceConnected(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean("Scheduling.DBusSchedQoS.ServiceConnectionSuccess",
                            success);
  if (!success) {
    LOG_IF(ERROR, !is_dbus_down_) << "resourced service is not available";
    is_dbus_down_ = true;
    main_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DBusSchedQOSStateHandler::WaitForResourcedAvailable,
                       weak_ptr_factory_.GetWeakPtr()),
        kRetryServiceMonitorInterval);
    return;
  }
  is_dbus_down_ = false;

  DCHECK(!is_connected_);
  if (is_connected_) {
    LOG(ERROR)
        << "DBusSchedQOSStateHandler::OnServiceConnected while it is connected";
    return;
  }
  is_connected_ = true;

  std::vector<std::pair<base::ProcessId, ProcessState>> preconnect_requests;
  // Copy entries in process_state_map_ to local vector to minimize the
  // locked section.
  // SetProcessPriority() calls just before/after this locked section do not
  // cause incoherence because both OnServiceConnected() and
  // SetProcessPriorityOnThread() run on the same main thread.
  {
    base::AutoLock lock(process_state_map_lock_);
    preconnect_requests.reserve(process_state_map_.size());
    for (auto& entry : process_state_map_) {
      preconnect_requests.push_back(
          {entry.first,
           ProcessState(entry.second.priority, entry.second.need_retry)});
      preconnect_requests.back().second.preconnected_thread_types.swap(
          entry.second.preconnected_thread_types);
      entry.second.need_retry = false;
    }
  }

  for (const auto& request : preconnect_requests) {
    base::ProcessId process_id = request.first;
    if (request.second.need_retry) {
      SetProcessPriorityOnThread(process_id, request.second.priority);
    }
    for (const auto& thread_entry : request.second.preconnected_thread_types) {
      SetThreadTypeOnThread(process_id, thread_entry.first,
                            thread_entry.second);
    }
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
  base::ElapsedTimer elapsed_timer;
  ash::ResourcedClient::Get()->SetProcessState(
      process_id, state,
      base::BindOnce(&DBusSchedQOSStateHandler::OnSetProcessPriorityFinish,
                     weak_ptr_factory_.GetWeakPtr(), process_id, priority,
                     std::move(elapsed_timer),
                     system::ProcStatFile(process_id)));
}

void DBusSchedQOSStateHandler::OnSetProcessPriorityFinish(
    base::ProcessId process_id,
    base::Process::Priority priority,
    base::ElapsedTimer elapsed_timer,
    system::ProcStatFile stat_file,
    dbus::DBusResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool success = result == dbus::DBusResult::kSuccess;
  const bool is_pid_reused =
      IsPidReused(std::move(stat_file), process_id, success);
  LOG_IF(ERROR, is_pid_reused) << "PID reuse detected for pid " << process_id;
  base::UmaHistogramEnumeration(
      "Scheduling.DBusSchedQoS.PidReusedOnSetProcessState",
      GetPidReuseResult(is_pid_reused, success));

  if (success) {
    base::UmaHistogramMicrosecondsTimes(
        "Scheduling.DBusSchedQoS.SetProcessStateLatency",
        elapsed_timer.Elapsed());
  } else {
    LOG(ERROR) << "set process state via resourced failed for pid "
               << process_id << " to " << static_cast<int>(priority)
               << " : DBusResult: " << static_cast<int>(result);
  }

  CheckResourcedDisconnected(result);
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

void DBusSchedQOSStateHandler::SetThreadTypeOnThread(
    base::ProcessId process_id,
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_) {
    AddThreadRetryEntry(process_id, thread_id, thread_type);
    return;
  }
  resource_manager::ThreadState state =
      resource_manager::ThreadState::kBalanced;
  switch (thread_type) {
    case base::ThreadType::kBackground:
      state = resource_manager::ThreadState::kBackground;
      break;
    case base::ThreadType::kUtility:
      state = resource_manager::ThreadState::kUtility;
      break;
    case base::ThreadType::kResourceEfficient:
      state = resource_manager::ThreadState::kEco;
      break;
    case base::ThreadType::kDefault:
      state = resource_manager::ThreadState::kBalanced;
      break;
    case base::ThreadType::kDisplayCritical:
      state = resource_manager::ThreadState::kUrgent;
      break;
    case base::ThreadType::kRealtimeAudio:
      state = resource_manager::ThreadState::kUrgentBursty;
      break;
  }
  base::ElapsedTimer elapsed_timer;
  ash::ResourcedClient::Get()->SetThreadState(
      process_id, thread_id, state,
      base::BindOnce(&DBusSchedQOSStateHandler::OnSetThreadTypeFinish,
                     weak_ptr_factory_.GetWeakPtr(), process_id, thread_id,
                     thread_type, std::move(elapsed_timer),
                     system::ProcStatFile(thread_id)));
}

void DBusSchedQOSStateHandler::OnSetThreadTypeFinish(
    base::ProcessId process_id,
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type,
    base::ElapsedTimer elapsed_timer,
    system::ProcStatFile stat_file,
    dbus::DBusResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool success = result == dbus::DBusResult::kSuccess;
  const bool is_pid_reused =
      IsPidReused(std::move(stat_file), thread_id, success);
  LOG_IF(ERROR, is_pid_reused)
      << "PID reuse detected for thread id " << thread_id;
  base::UmaHistogramEnumeration(
      "Scheduling.DBusSchedQoS.PidReusedOnSetThreadState",
      GetPidReuseResult(is_pid_reused, success));

  if (result == dbus::DBusResult::kSuccess) {
    base::UmaHistogramMicrosecondsTimes(
        "Scheduling.DBusSchedQoS.SetThreadStateLatency",
        elapsed_timer.Elapsed());
  } else {
    LOG(ERROR) << "set thread state via resourced failed for tid " << thread_id
               << " to " << static_cast<int>(thread_type)
               << " : DBusResult: " << static_cast<int>(result);
  }

  CheckResourcedDisconnected(result);
  if (!is_connected_) {
    AddThreadRetryEntry(process_id, thread_id, thread_type);
  }
}

void DBusSchedQOSStateHandler::AddThreadRetryEntry(
    base::ProcessId process_id,
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(process_state_map_lock_);
  if (auto entry = process_state_map_.find(process_id);
      entry != process_state_map_.end()) {
    entry->second.preconnected_thread_types[thread_id] = thread_type;
  }
}

}  // namespace ash
