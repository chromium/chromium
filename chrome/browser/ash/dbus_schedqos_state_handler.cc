// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus_schedqos_state_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
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

bool DBusSchedQOSStateHandler::SetProcessPriority(
    base::ProcessId process_id,
    base::Process::Priority priority) {
  return main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBusSchedQOSStateHandler::SetProcessPriorityOnThread,
                     weak_ptr_factory_.GetWeakPtr(), process_id, priority));
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
